// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_Snapshots.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Services/MCPSnapshotStore.h"
#include "MCPToolRegistry.h"
#include "Reflection/MCPReflectionCore.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/EngineVersion.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Switch.h"
#include "Services/MCPBlueprintGraphExtractor.h"
#endif

#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Texture.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPSnapshot, Log, All);

// ============================================================================
// Helpers
// ============================================================================
namespace
{
	/** Build a minimal asset item JSON from FAssetData (reused from asset tools logic). */
	TSharedPtr<FJsonObject> MakeSnapshotAssetItem(const FAssetData& AssetData)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("asset_path"), AssetData.GetObjectPathString());
		Item->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.ToString());
		Item->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());

		TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
		AssetData.TagsAndValues.ForEach([&TagsObj](TPair<FName, FAssetTagValueRef> Pair)
		{
			TagsObj->SetStringField(Pair.Key.ToString(), Pair.Value.AsString());
			return true;
		});
		Item->SetObjectField(TEXT("tags"), TagsObj);

		return Item;
	}

	/** Collect all assets matching scope paths. */
	TArray<FAssetData> CollectAssetsForScope(const TArray<FString>& ScopePaths)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		TArray<FAssetData> AllAssets;

		if (ScopePaths.Num() == 0)
		{
			// No paths specified — collect everything under /Game
			FARFilter Filter;
			Filter.PackagePaths.Add(FName(TEXT("/Game")));
			Filter.bRecursivePaths = true;
			AssetRegistry.GetAssets(Filter, AllAssets);
		}
		else
		{
			for (const FString& PathPrefix : ScopePaths)
			{
				FARFilter Filter;
				Filter.PackagePaths.Add(FName(*PathPrefix));
				Filter.bRecursivePaths = true;

				TArray<FAssetData> PathAssets;
				AssetRegistry.GetAssets(Filter, PathAssets);
				AllAssets.Append(PathAssets);
			}
		}

		// De-duplicate by object path
		TSet<FString> Seen;
		TArray<FAssetData> Unique;
		for (const FAssetData& AD : AllAssets)
		{
			FString Path = AD.GetObjectPathString();
			if (!Seen.Contains(Path))
			{
				Seen.Add(Path);
				Unique.Add(AD);
			}
		}

		// Sort for stable output
		Unique.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});

		return Unique;
	}

	/** Extract path field name based on select type. */
	FString GetPathFieldForSelect(const FString& Select)
	{
		if (Select == TEXT("assets")) return TEXT("asset_path");
		if (Select == TEXT("blueprints")) return TEXT("bp_path");
		if (Select == TEXT("materials")) return TEXT("material_path");
		if (Select == TEXT("classes")) return TEXT("class_name");
		return TEXT("asset_path");
	}

	/** Check if a JSON item matches a filter. */
	bool MatchesFilter(const TSharedPtr<FJsonObject>& Item, const TSharedPtr<FJsonObject>& Filter, const FString& Select)
	{
		if (!Filter.IsValid()) return true;

		// path_prefix
		FString PathPrefix;
		if (Filter->TryGetStringField(TEXT("path_prefix"), PathPrefix))
		{
			FString PathField = GetPathFieldForSelect(Select);
			FString ItemPath;
			if (!Item->TryGetStringField(PathField, ItemPath) || !ItemPath.StartsWith(PathPrefix))
				return false;
		}

		// name_contains
		FString NameContains;
		if (Filter->TryGetStringField(TEXT("name_contains"), NameContains))
		{
			FString PathField = GetPathFieldForSelect(Select);
			FString ItemPath;
			if (Item->TryGetStringField(PathField, ItemPath))
			{
				// Extract the asset name from path (last segment after .)
				FString AssetName;
				int32 DotIndex;
				if (ItemPath.FindLastChar('.', DotIndex))
				{
					AssetName = ItemPath.Mid(DotIndex + 1);
				}
				else
				{
					AssetName = ItemPath;
				}
				if (!AssetName.Contains(NameContains))
					return false;
			}
			else
			{
				return false;
			}
		}

		// class (assets only)
		FString ClassFilter;
		if (Filter->TryGetStringField(TEXT("class"), ClassFilter) && Select == TEXT("assets"))
		{
			FString AssetClass;
			if (!Item->TryGetStringField(TEXT("asset_class"), AssetClass) || AssetClass != ClassFilter)
				return false;
		}

		// has_tag (assets only)
		FString HasTag;
		if (Filter->TryGetStringField(TEXT("has_tag"), HasTag) && Select == TEXT("assets"))
		{
			const TSharedPtr<FJsonObject>* Tags = nullptr;
			if (!Item->TryGetObjectField(TEXT("tags"), Tags) || !(*Tags)->HasField(HasTag))
				return false;
		}

		// tag_value (assets only)
		const TSharedPtr<FJsonObject>* TagValueFilter = nullptr;
		if (Filter->TryGetObjectField(TEXT("tag_value"), TagValueFilter) && Select == TEXT("assets"))
		{
			FString Key, Value;
			(*TagValueFilter)->TryGetStringField(TEXT("key"), Key);
			(*TagValueFilter)->TryGetStringField(TEXT("value"), Value);

			const TSharedPtr<FJsonObject>* Tags = nullptr;
			if (!Item->TryGetObjectField(TEXT("tags"), Tags))
				return false;

			FString TagVal;
			if (!(*Tags)->TryGetStringField(Key, TagVal) || TagVal != Value)
				return false;
		}

		// has_property (classes only)
		FString HasProperty;
		if (Filter->TryGetStringField(TEXT("has_property"), HasProperty) && Select == TEXT("classes"))
		{
			const TArray<TSharedPtr<FJsonValue>>* PropsArr = nullptr;
			if (!Item->TryGetArrayField(TEXT("properties"), PropsArr))
				return false;

			bool bFound = false;
			for (const auto& PropVal : *PropsArr)
			{
				const TSharedPtr<FJsonObject>* PropObj = nullptr;
				if (PropVal->TryGetObject(PropObj))
				{
					FString PropName;
					if ((*PropObj)->TryGetStringField(TEXT("name"), PropName) && PropName == HasProperty)
					{
						bFound = true;
						break;
					}
				}
			}
			if (!bFound) return false;
		}

		// has_function (classes only)
		FString HasFunction;
		if (Filter->TryGetStringField(TEXT("has_function"), HasFunction) && Select == TEXT("classes"))
		{
			const TArray<TSharedPtr<FJsonValue>>* FuncsArr = nullptr;
			if (!Item->TryGetArrayField(TEXT("functions"), FuncsArr))
				return false;

			bool bFound = false;
			for (const auto& FuncVal : *FuncsArr)
			{
				const TSharedPtr<FJsonObject>* FuncObj = nullptr;
				if (FuncVal->TryGetObject(FuncObj))
				{
					FString FuncName;
					if ((*FuncObj)->TryGetStringField(TEXT("name"), FuncName) && FuncName == HasFunction)
					{
						bFound = true;
						break;
					}
				}
			}
			if (!bFound) return false;
		}

		// param_name (materials only)
		FString ParamName;
		if (Filter->TryGetStringField(TEXT("param_name"), ParamName) && Select == TEXT("materials"))
		{
			const TArray<TSharedPtr<FJsonValue>>* ParamsArr = nullptr;
			if (!Item->TryGetArrayField(TEXT("params"), ParamsArr))
				return false;

			bool bFound = false;
			for (const auto& ParamVal : *ParamsArr)
			{
				const TSharedPtr<FJsonObject>* ParamObj = nullptr;
				if (ParamVal->TryGetObject(ParamObj))
				{
					FString PName;
					if ((*ParamObj)->TryGetStringField(TEXT("name"), PName) && PName == ParamName)
					{
						bFound = true;
						break;
					}
				}
			}
			if (!bFound) return false;
		}

		return true;
	}

	/** Apply field projection to a JSON object. */
	TSharedPtr<FJsonObject> ProjectFields(const TSharedPtr<FJsonObject>& Item, const TArray<FString>& Fields)
	{
		if (Fields.Num() == 0) return Item;

		TSharedPtr<FJsonObject> Projected = MakeShared<FJsonObject>();
		for (const FString& Field : Fields)
		{
			if (Item->HasField(Field))
			{
				Projected->SetField(Field, Item->TryGetField(Field));
			}
		}
		return Projected;
	}
}

// ============================================================================
// ue_snapshot_build
// ============================================================================
namespace SnapshotBuild
{
	static constexpr const TCHAR* ToolName = TEXT("ue_snapshot_build");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Collect a snapshot of project state by scope and detail level. "
			 "Aggregates asset index, class schemas, blueprint summaries, and material params.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_snapshot_build");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/,
		FMCPSnapshotStore& SnapshotStore)
	{
		double StartTime = FPlatformTime::Seconds();

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.snapshot") }, {}, true);

		TArray<FString> Warnings;

		// Parse scope
		TArray<FString> ScopePaths;
		TArray<FString> ScopeClasses;
		{
			const TSharedPtr<FJsonObject>* ScopePtr = nullptr;
			if (Args.IsValid() && Args->TryGetObjectField(TEXT("scope"), ScopePtr) && ScopePtr)
			{
				ScopePaths = MCPToolExecution::GetStringArrayParam(*ScopePtr, TEXT("paths"));
				ScopeClasses = MCPToolExecution::GetStringArrayParam(*ScopePtr, TEXT("classes"));
			}
		}

		// Parse include levels
		TArray<FString> IncludeLevels;
		if (Args.IsValid())
		{
			IncludeLevels = MCPToolExecution::GetStringArrayParam(Args, TEXT("include"));
		}

		if (IncludeLevels.Num() == 0)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: include (array of level strings)"));
			return Envelope;
		}

		// Generate snapshot ID
		FString SnapshotId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

		// Initialize snapshot data
		TSharedPtr<FJsonObject> SnapshotData = MakeShared<FJsonObject>();

		int32 AssetsCount = 0;
		int32 ClassesCount = 0;
		int32 BpsCount = 0;
		int32 MaterialsCount = 0;

		// Collected assets (shared across levels)
		TArray<FAssetData> CollectedAssets;

		// ─── L0: Asset Index ───
		if (IncludeLevels.Contains(TEXT("L0")))
		{
			CollectedAssets = CollectAssetsForScope(ScopePaths);

			TArray<TSharedPtr<FJsonValue>> AssetItems;
			TSet<FName> TagKeys;

			for (const FAssetData& AD : CollectedAssets)
			{
				AssetItems.Add(MakeShared<FJsonValueObject>(MakeSnapshotAssetItem(AD)));

				AD.TagsAndValues.ForEach([&TagKeys](TPair<FName, FAssetTagValueRef> Pair)
				{
					TagKeys.Add(Pair.Key);
					return true;
				});
			}

			TArray<TSharedPtr<FJsonValue>> TagKeysArr;
			for (const FName& Key : TagKeys)
			{
				TagKeysArr.Add(MakeShared<FJsonValueString>(Key.ToString()));
			}

			TSharedPtr<FJsonObject> L0 = MakeShared<FJsonObject>();
			L0->SetArrayField(TEXT("items"), AssetItems);
			L0->SetArrayField(TEXT("tag_keys"), TagKeysArr);

			SnapshotData->SetObjectField(TEXT("L0_assets"), L0);
			AssetsCount = AssetItems.Num();
		}
		else
		{
			// Still need assets for L2/bp_summary/mat_params if requested
			if (IncludeLevels.Contains(TEXT("L2")) ||
				IncludeLevels.Contains(TEXT("bp_summary")) ||
				IncludeLevels.Contains(TEXT("mat_params")))
			{
				CollectedAssets = CollectAssetsForScope(ScopePaths);
			}
		}

		// ─── L1: Class Reflection ───
		if (IncludeLevels.Contains(TEXT("L1")))
		{
			TSet<FString> ClassNames;

			// Explicitly specified classes
			for (const FString& C : ScopeClasses)
			{
				ClassNames.Add(C);
			}

			// Blueprint generated classes from L0
			for (const FAssetData& AD : CollectedAssets)
			{
				FString GenClass;
				if (AD.GetTagValue(FName(TEXT("GeneratedClass")), GenClass))
				{
					ClassNames.Add(GenClass);
				}
			}

			TArray<TSharedPtr<FJsonValue>> ClassSchemas;

			for (const FString& ClassName : ClassNames)
			{
				UClass* Class = MCPReflection::ResolveClass(ClassName);
				if (!Class)
				{
					Warnings.Add(FString::Printf(TEXT("L1: Could not resolve class '%s'"), *ClassName));
					continue;
				}

				TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
				Schema->SetStringField(TEXT("class_name"), Class->GetPathName());

				// Properties
				TArray<TSharedPtr<FJsonValue>> PropsArr;
				for (TFieldIterator<FProperty> It(Class); It; ++It)
				{
					TSharedPtr<FJsonObject> PropDesc = MCPReflection::BuildPropertyDesc(*It, false);
					if (PropDesc.IsValid())
					{
						PropsArr.Add(MakeShared<FJsonValueObject>(PropDesc));
					}
				}
				Schema->SetArrayField(TEXT("properties"), PropsArr);

				// Functions (BlueprintCallable only)
				TArray<TSharedPtr<FJsonValue>> FuncsArr;
				for (TFieldIterator<UFunction> It(Class); It; ++It)
				{
					if (!((*It)->FunctionFlags & FUNC_BlueprintCallable))
						continue;

					TSharedPtr<FJsonObject> FuncDesc = MCPReflection::BuildFunctionDesc(*It);
					if (FuncDesc.IsValid())
					{
						FuncsArr.Add(MakeShared<FJsonValueObject>(FuncDesc));
					}
				}
				Schema->SetArrayField(TEXT("functions"), FuncsArr);

				ClassSchemas.Add(MakeShared<FJsonValueObject>(Schema));
			}

			TSharedPtr<FJsonObject> L1 = MakeShared<FJsonObject>();
			L1->SetArrayField(TEXT("schemas"), ClassSchemas);
			SnapshotData->SetObjectField(TEXT("L1_classes"), L1);
			ClassesCount = ClassSchemas.Num();
		}

		// ─── L2: Blueprints ───
#if WITH_EDITOR
		TArray<TSharedPtr<FJsonValue>> BPSummaries;
		if (IncludeLevels.Contains(TEXT("L2")) || IncludeLevels.Contains(TEXT("bp_summary")))
		{
			// Filter blueprint assets from collected assets
			for (const FAssetData& AD : CollectedAssets)
			{
				if (AD.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName())
					continue;

				UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AD.GetObjectPathString());
				if (!BP)
				{
					Warnings.Add(FString::Printf(TEXT("L2: Failed to load blueprint '%s'"), *AD.GetObjectPathString()));
					continue;
				}

				TSharedPtr<FJsonObject> BPSummary = MakeShared<FJsonObject>();
				BPSummary->SetStringField(TEXT("bp_path"), AD.GetObjectPathString());

				if (BP->GeneratedClass)
				{
					BPSummary->SetStringField(TEXT("generated_class"), BP->GeneratedClass->GetPathName());
				}
				else
				{
					BPSummary->SetField(TEXT("generated_class"), MakeShared<FJsonValueNull>());
				}

				// Graphs list
				TArray<TSharedPtr<FJsonValue>> GraphsArr;
				auto AddGraphs = [&GraphsArr](const TArray<UEdGraph*>& Graphs, const TCHAR* TypeStr)
				{
					for (UEdGraph* Graph : Graphs)
					{
						if (!Graph) continue;
						TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
						GraphObj->SetStringField(TEXT("name"), Graph->GetName());
						GraphObj->SetStringField(TEXT("type"), TypeStr);
						GraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
						GraphsArr.Add(MakeShared<FJsonValueObject>(GraphObj));
					}
				};
				AddGraphs(BP->UbergraphPages, TEXT("EventGraph"));
				AddGraphs(BP->FunctionGraphs, TEXT("Function"));
				AddGraphs(BP->MacroGraphs, TEXT("Macro"));
				BPSummary->SetArrayField(TEXT("graphs"), GraphsArr);

				// ─── bp_summary: Function summaries ───
				if (IncludeLevels.Contains(TEXT("bp_summary")))
				{
					TArray<TSharedPtr<FJsonValue>> FuncSummaries;

					for (UEdGraph* FuncGraph : BP->FunctionGraphs)
					{
						if (!FuncGraph) continue;

						TSharedPtr<FJsonObject> FuncSummary = MakeShared<FJsonObject>();
						FuncSummary->SetStringField(TEXT("function_name"), FuncGraph->GetName());

						TArray<TSharedPtr<FJsonValue>> Steps;
						TArray<TSharedPtr<FJsonValue>> Calls;
						TArray<TSharedPtr<FJsonValue>> Reads;
						TArray<TSharedPtr<FJsonValue>> Writes;
						TArray<TSharedPtr<FJsonValue>> Branches;

						UK2Node_FunctionEntry* EntryNode = nullptr;

						for (UEdGraphNode* Node : FuncGraph->Nodes)
						{
							if (!Node) continue;

							if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
							{
								Calls.Add(MakeShared<FJsonValueString>(CallNode->GetFunctionName().ToString()));
							}
							else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
							{
								Reads.Add(MakeShared<FJsonValueString>(GetNode->GetVarName().ToString()));
							}
							else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
							{
								Writes.Add(MakeShared<FJsonValueString>(SetNode->GetVarName().ToString()));
							}
							else if (Cast<UK2Node_IfThenElse>(Node) || Cast<UK2Node_Switch>(Node))
							{
								FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
								Branches.Add(MakeShared<FJsonValueString>(Title));
							}

							if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
							{
								EntryNode = Entry;
							}
						}

						// Generate steps from exec pin chain
						if (EntryNode)
						{
							TSet<UEdGraphNode*> Visited;
							UEdGraphNode* Current = EntryNode;
							while (Current && !Visited.Contains(Current))
							{
								Visited.Add(Current);
								FString StepTitle = Current->GetNodeTitle(ENodeTitleType::ListView).ToString();
								if (!StepTitle.IsEmpty())
								{
									Steps.Add(MakeShared<FJsonValueString>(StepTitle));
								}
								UEdGraphNode* Next = nullptr;
								for (UEdGraphPin* Pin : Current->Pins)
								{
									if (!Pin || Pin->Direction != EGPD_Output) continue;
									if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
									if (Cast<UK2Node_IfThenElse>(Current) && Pin->PinName != TEXT("Then")) continue;
									if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0])
									{
										Next = Pin->LinkedTo[0]->GetOwningNode();
										break;
									}
								}
								Current = Next;
							}
						}

						FuncSummary->SetArrayField(TEXT("steps"), Steps);
						FuncSummary->SetArrayField(TEXT("calls"), Calls);
						FuncSummary->SetArrayField(TEXT("reads"), Reads);
						FuncSummary->SetArrayField(TEXT("writes"), Writes);
						FuncSummary->SetArrayField(TEXT("branches"), Branches);

						FuncSummaries.Add(MakeShared<FJsonValueObject>(FuncSummary));
					}

					BPSummary->SetArrayField(TEXT("function_summaries"), FuncSummaries);
				}

				BPSummaries.Add(MakeShared<FJsonValueObject>(BPSummary));
			}

			TSharedPtr<FJsonObject> L2BP = MakeShared<FJsonObject>();
			L2BP->SetArrayField(TEXT("summaries"), BPSummaries);
			SnapshotData->SetObjectField(TEXT("L2_blueprints"), L2BP);
			BpsCount = BPSummaries.Num();
		}
#else
		if (IncludeLevels.Contains(TEXT("L2")) || IncludeLevels.Contains(TEXT("bp_summary")))
		{
			Warnings.Add(TEXT("L2/bp_summary requires Editor build (WITH_EDITOR)"));
		}
#endif

		// ─── mat_params: Material Parameters ───
		if (IncludeLevels.Contains(TEXT("mat_params")))
		{
			TArray<TSharedPtr<FJsonValue>> MatItems;

			for (const FAssetData& AD : CollectedAssets)
			{
				FString ClassStr = AD.AssetClassPath.ToString();
				bool bIsMaterial = ClassStr.Contains(TEXT("Material"));
				if (!bIsMaterial) continue;

				UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *AD.GetObjectPathString());
				if (!Mat) continue;

				TSharedPtr<FJsonObject> MatItem = MakeShared<FJsonObject>();
				MatItem->SetStringField(TEXT("material_path"), Mat->GetPathName());

				UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Mat);
				if (MIC)
				{
					MatItem->SetStringField(TEXT("material_class"), TEXT("MaterialInstanceConstant"));
				}
				else if (Cast<UMaterial>(Mat))
				{
					MatItem->SetStringField(TEXT("material_class"), TEXT("Material"));
				}
				else
				{
					MatItem->SetStringField(TEXT("material_class"), TEXT("MaterialInstance"));
				}

				TArray<TSharedPtr<FJsonValue>> ParamsArr;

				// Scalar parameters
				{
					TArray<FMaterialParameterInfo> ScalarInfos;
					TArray<FGuid> ScalarGuids;
					Mat->GetAllScalarParameterInfo(ScalarInfos, ScalarGuids);
					for (const FMaterialParameterInfo& Info : ScalarInfos)
					{
						float ScalarValue = 0.f;
						Mat->GetScalarParameterValue(Info.Name, ScalarValue);

						TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
						Param->SetStringField(TEXT("name"), Info.Name.ToString());
						Param->SetStringField(TEXT("type"), TEXT("scalar"));
						Param->SetNumberField(TEXT("value"), ScalarValue);

						if (MIC)
						{
							bool bOverridden = false;
							for (const FScalarParameterValue& SPV : MIC->ScalarParameterValues)
							{
								if (SPV.ParameterInfo.Name == Info.Name)
								{
									bOverridden = true;
									break;
								}
							}
							Param->SetBoolField(TEXT("overridden"), bOverridden);
						}

						ParamsArr.Add(MakeShared<FJsonValueObject>(Param));
					}
				}

				// Vector parameters
				{
					TArray<FMaterialParameterInfo> VectorInfos;
					TArray<FGuid> VectorGuids;
					Mat->GetAllVectorParameterInfo(VectorInfos, VectorGuids);
					for (const FMaterialParameterInfo& Info : VectorInfos)
					{
						FLinearColor VectorValue = FLinearColor::Black;
						Mat->GetVectorParameterValue(Info.Name, VectorValue);

						TSharedPtr<FJsonObject> ValueObj = MakeShared<FJsonObject>();
						ValueObj->SetNumberField(TEXT("r"), VectorValue.R);
						ValueObj->SetNumberField(TEXT("g"), VectorValue.G);
						ValueObj->SetNumberField(TEXT("b"), VectorValue.B);
						ValueObj->SetNumberField(TEXT("a"), VectorValue.A);

						TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
						Param->SetStringField(TEXT("name"), Info.Name.ToString());
						Param->SetStringField(TEXT("type"), TEXT("vector"));
						Param->SetObjectField(TEXT("value"), ValueObj);

						if (MIC)
						{
							bool bOverridden = false;
							for (const FVectorParameterValue& VPV : MIC->VectorParameterValues)
							{
								if (VPV.ParameterInfo.Name == Info.Name)
								{
									bOverridden = true;
									break;
								}
							}
							Param->SetBoolField(TEXT("overridden"), bOverridden);
						}

						ParamsArr.Add(MakeShared<FJsonValueObject>(Param));
					}
				}

				// Texture parameters
				{
					TArray<FMaterialParameterInfo> TexInfos;
					TArray<FGuid> TexGuids;
					Mat->GetAllTextureParameterInfo(TexInfos, TexGuids);
					for (const FMaterialParameterInfo& Info : TexInfos)
					{
						UTexture* TexValue = nullptr;
						Mat->GetTextureParameterValue(Info.Name, TexValue);

						TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
						Param->SetStringField(TEXT("name"), Info.Name.ToString());
						Param->SetStringField(TEXT("type"), TEXT("texture"));
						if (TexValue)
						{
							Param->SetStringField(TEXT("value"), TexValue->GetPathName());
						}
						else
						{
							Param->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
						}

						if (MIC)
						{
							bool bOverridden = false;
							for (const FTextureParameterValue& TPV : MIC->TextureParameterValues)
							{
								if (TPV.ParameterInfo.Name == Info.Name)
								{
									bOverridden = true;
									break;
								}
							}
							Param->SetBoolField(TEXT("overridden"), bOverridden);
						}

						ParamsArr.Add(MakeShared<FJsonValueObject>(Param));
					}
				}

				MatItem->SetArrayField(TEXT("params"), ParamsArr);
				MatItems.Add(MakeShared<FJsonValueObject>(MatItem));
			}

			TSharedPtr<FJsonObject> L2Mat = MakeShared<FJsonObject>();
			L2Mat->SetArrayField(TEXT("params"), MatItems);
			SnapshotData->SetObjectField(TEXT("L2_materials"), L2Mat);
			MaterialsCount = MatItems.Num();
		}

		// Calculate stats
		double EndTime = FPlatformTime::Seconds();
		double CollectTimeMs = (EndTime - StartTime) * 1000.0;

		// Serialize to get size
		FString DataStr;
		{
			auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&DataStr);
			FJsonSerializer::Serialize(SnapshotData.ToSharedRef(), Writer);
		}
		int64 SizeBytes = DataStr.Len() * sizeof(TCHAR);

		// Build meta
		FMCPSnapshotMeta Meta;
		Meta.Id = SnapshotId;
		Meta.CreatedAt = FDateTime::UtcNow();
		Meta.ScopePaths = ScopePaths;
		Meta.ScopeClasses = ScopeClasses;
		Meta.IncludeLevels = IncludeLevels;
		Meta.AssetsCount = AssetsCount;
		Meta.ClassesCount = ClassesCount;
		Meta.BpsCount = BpsCount;
		Meta.MaterialsCount = MaterialsCount;
		Meta.SizeBytes = SizeBytes;
		Meta.CollectTimeMs = CollectTimeMs;
		Meta.EngineVersion = FEngineVersion::Current().ToString();
		Meta.ProjectName = FApp::GetProjectName();

		// Add meta to snapshot data
		SnapshotData->SetObjectField(TEXT("meta"), Meta.ToJson());

		// Persist
		SnapshotStore.Store(SnapshotId, Meta, SnapshotData);

		// Build response envelope
		Envelope->SetStringField(TEXT("snapshot_id"), SnapshotId);

		TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
		StatsObj->SetNumberField(TEXT("assets_count"), AssetsCount);
		StatsObj->SetNumberField(TEXT("classes_count"), ClassesCount);
		StatsObj->SetNumberField(TEXT("bps_count"), BpsCount);
		StatsObj->SetNumberField(TEXT("materials_count"), MaterialsCount);
		StatsObj->SetNumberField(TEXT("size_bytes"), static_cast<double>(SizeBytes));
		StatsObj->SetNumberField(TEXT("collect_time_ms"), CollectTimeMs);
		Envelope->SetObjectField(TEXT("stats"), StatsObj);

		// Add warnings
		if (Warnings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> WarningsArr;
			for (const FString& W : Warnings)
			{
				WarningsArr.Add(MakeShared<FJsonValueString>(W));
			}
			Envelope->SetArrayField(TEXT("warnings"), WarningsArr);
		}

		UE_LOG(LogMCPSnapshot, Log, TEXT("Snapshot built: id=%s, assets=%d, classes=%d, bps=%d, mats=%d, %.1fms"),
			*SnapshotId, AssetsCount, ClassesCount, BpsCount, MaterialsCount, CollectTimeMs);

		return Envelope;
	}
}

// ============================================================================
// ue_snapshot_list
// ============================================================================
namespace SnapshotList
{
	static constexpr const TCHAR* ToolName = TEXT("ue_snapshot_list");
	static constexpr const TCHAR* ToolDescription =
		TEXT("List all available snapshots with metadata (scope, stats, timestamps).");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_snapshot_list");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& /*Args*/,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/,
		FMCPSnapshotStore& SnapshotStore)
	{
		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.snapshot") }, {}, true);

		TArray<FMCPSnapshotMeta> AllMeta = SnapshotStore.List();

		TArray<TSharedPtr<FJsonValue>> SnapshotsArr;
		for (const FMCPSnapshotMeta& Meta : AllMeta)
		{
			SnapshotsArr.Add(MakeShared<FJsonValueObject>(Meta.ToJson()));
		}

		Envelope->SetArrayField(TEXT("snapshots"), SnapshotsArr);

		return Envelope;
	}
}

// ============================================================================
// ue_snapshot_query
// ============================================================================
namespace SnapshotQuery
{
	static constexpr const TCHAR* ToolName = TEXT("ue_snapshot_query");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Query a snapshot with structured DSL (select, filter, fields, sort, pagination). "
			 "Results returned as resource.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_snapshot_query");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore,
		FMCPSnapshotStore& SnapshotStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.snapshot") }, {}, true);

		FString SnapshotId = MCPToolExecution::GetStringParam(Args, TEXT("snapshot_id"));
		if (SnapshotId.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: snapshot_id"));
			return Envelope;
		}

		// Get snapshot data
		TSharedPtr<FJsonObject> SnapshotData = SnapshotStore.Get(SnapshotId);
		if (!SnapshotData.IsValid())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Snapshot not found: %s"), *SnapshotId));
			return Envelope;
		}

		// Parse query DSL
		const TSharedPtr<FJsonObject>* QueryPtr = nullptr;
		if (!Args->TryGetObjectField(TEXT("query"), QueryPtr) || !QueryPtr)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: query"));
			return Envelope;
		}
		TSharedPtr<FJsonObject> Query = *QueryPtr;

		FString Select = MCPToolExecution::GetStringParam(Query, TEXT("select"), TEXT("assets"));

		const TSharedPtr<FJsonObject>* FilterPtr = nullptr;
		TSharedPtr<FJsonObject> Filter;
		if (Query->TryGetObjectField(TEXT("filter"), FilterPtr) && FilterPtr)
		{
			Filter = *FilterPtr;
		}

		TArray<FString> Fields = MCPToolExecution::GetStringArrayParam(Query, TEXT("fields"));
		FString SortBy = MCPToolExecution::GetStringParam(Query, TEXT("sort_by"));
		int32 Limit = MCPToolExecution::GetIntParam(Query, TEXT("limit"), 200);
		int32 Offset = MCPToolExecution::GetIntParam(Query, TEXT("offset"), 0);

		// Determine source array
		TArray<TSharedPtr<FJsonValue>> SourceItems;
		int32 TotalInSnapshot = 0;

		if (Select == TEXT("all"))
		{
			// Return complete snapshot data as resource
			FString Uri = ResourceStore.Store(SnapshotData);

			TSharedPtr<FJsonObject> ResourceObj = MakeShared<FJsonObject>();
			ResourceObj->SetStringField(TEXT("uri"), Uri);
			ResourceObj->SetStringField(TEXT("mime_type"), TEXT("application/json"));
			ResourceObj->SetStringField(TEXT("description"), TEXT("Complete snapshot data"));

			Envelope->SetObjectField(TEXT("resource"), ResourceObj);
			Envelope->SetNumberField(TEXT("result_count"), 1);
			Envelope->SetNumberField(TEXT("total_in_snapshot"), 1);

			return Envelope;
		}
		else if (Select == TEXT("assets"))
		{
			const TSharedPtr<FJsonObject>* L0Ptr = nullptr;
			if (SnapshotData->TryGetObjectField(TEXT("L0_assets"), L0Ptr) && L0Ptr)
			{
				const TArray<TSharedPtr<FJsonValue>>* ItemsPtr = nullptr;
				if ((*L0Ptr)->TryGetArrayField(TEXT("items"), ItemsPtr))
				{
					SourceItems = *ItemsPtr;
				}
			}
		}
		else if (Select == TEXT("classes"))
		{
			const TSharedPtr<FJsonObject>* L1Ptr = nullptr;
			if (SnapshotData->TryGetObjectField(TEXT("L1_classes"), L1Ptr) && L1Ptr)
			{
				const TArray<TSharedPtr<FJsonValue>>* SchemasPtr = nullptr;
				if ((*L1Ptr)->TryGetArrayField(TEXT("schemas"), SchemasPtr))
				{
					SourceItems = *SchemasPtr;
				}
			}
		}
		else if (Select == TEXT("blueprints"))
		{
			const TSharedPtr<FJsonObject>* L2Ptr = nullptr;
			if (SnapshotData->TryGetObjectField(TEXT("L2_blueprints"), L2Ptr) && L2Ptr)
			{
				const TArray<TSharedPtr<FJsonValue>>* SummariesPtr = nullptr;
				if ((*L2Ptr)->TryGetArrayField(TEXT("summaries"), SummariesPtr))
				{
					SourceItems = *SummariesPtr;
				}
			}
		}
		else if (Select == TEXT("materials"))
		{
			const TSharedPtr<FJsonObject>* L2MatPtr = nullptr;
			if (SnapshotData->TryGetObjectField(TEXT("L2_materials"), L2MatPtr) && L2MatPtr)
			{
				const TArray<TSharedPtr<FJsonValue>>* ParamsPtr = nullptr;
				if ((*L2MatPtr)->TryGetArrayField(TEXT("params"), ParamsPtr))
				{
					SourceItems = *ParamsPtr;
				}
			}
		}
		else
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				FString::Printf(TEXT("Invalid select value: '%s'. Must be one of: assets, classes, blueprints, materials, all"), *Select));
			return Envelope;
		}

		TotalInSnapshot = SourceItems.Num();

		// Apply filter
		TArray<TSharedPtr<FJsonValue>> FilteredItems;
		for (const TSharedPtr<FJsonValue>& ItemVal : SourceItems)
		{
			const TSharedPtr<FJsonObject>* ItemObj = nullptr;
			if (!ItemVal->TryGetObject(ItemObj))
				continue;

			if (MatchesFilter(*ItemObj, Filter, Select))
			{
				FilteredItems.Add(ItemVal);
			}
		}

		// Sort
		if (!SortBy.IsEmpty())
		{
			FilteredItems.Sort([&SortBy](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
			{
				const TSharedPtr<FJsonObject>* AObj = nullptr;
				const TSharedPtr<FJsonObject>* BObj = nullptr;
				if (!A->TryGetObject(AObj) || !B->TryGetObject(BObj))
					return false;

				FString AVal, BVal;
				(*AObj)->TryGetStringField(SortBy, AVal);
				(*BObj)->TryGetStringField(SortBy, BVal);
				return AVal < BVal;
			});
		}

		// Paginate
		int32 Total = FilteredItems.Num();
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + Limit, Total);

		TArray<TSharedPtr<FJsonValue>> PagedItems;
		for (int32 i = Start; i < End; ++i)
		{
			// Apply field projection if specified
			if (Fields.Num() > 0)
			{
				const TSharedPtr<FJsonObject>* ItemObj = nullptr;
				if (FilteredItems[i]->TryGetObject(ItemObj))
				{
					TSharedPtr<FJsonObject> Projected = ProjectFields(*ItemObj, Fields);
					PagedItems.Add(MakeShared<FJsonValueObject>(Projected));
				}
			}
			else
			{
				PagedItems.Add(FilteredItems[i]);
			}
		}

		// Build result and store as resource
		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetObjectField(TEXT("query"), Query);
		ResultData->SetStringField(TEXT("snapshot_id"), SnapshotId);
		ResultData->SetNumberField(TEXT("result_count"), PagedItems.Num());
		ResultData->SetArrayField(TEXT("items"), PagedItems);

		FString Uri = ResourceStore.Store(ResultData);

		TSharedPtr<FJsonObject> ResourceObj = MakeShared<FJsonObject>();
		ResourceObj->SetStringField(TEXT("uri"), Uri);
		ResourceObj->SetStringField(TEXT("mime_type"), TEXT("application/json"));
		ResourceObj->SetStringField(TEXT("description"),
			FString::Printf(TEXT("Snapshot query result: %d %s matching filter"), PagedItems.Num(), *Select));

		Envelope->SetObjectField(TEXT("resource"), ResourceObj);
		Envelope->SetNumberField(TEXT("result_count"), PagedItems.Num());
		Envelope->SetNumberField(TEXT("total_in_snapshot"), TotalInSnapshot);

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_Snapshots::RegisterAll(
	FMCPToolRegistry& Registry,
	FMCPRuntimeState& RuntimeState,
	FMCPResourceStore& ResourceStore,
	FMCPSnapshotStore& SnapshotStore)
{
	// Helper for registering GameThread tools with 4 service dependencies
	auto RegisterSnapshotTool = [&](const TCHAR* Name, const TCHAR* Desc, const TCHAR* SchemaDef,
		TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&, FMCPRuntimeState&, FMCPResourceStore&, FMCPSnapshotStore&)> Impl)
	{
		TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(SchemaDef);
		FMCPToolRegistration Reg;
		Reg.Descriptor.Name = Name;
		Reg.Descriptor.Description = Desc;
		Reg.Descriptor.InputSchema = Schema;
		Reg.Execute = [&RuntimeState, &ResourceStore, &SnapshotStore, Impl](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
		{
			return MCPToolExecution::RunOnGameThread([&]()
			{
				return Impl(Args, RuntimeState, ResourceStore, SnapshotStore);
			});
		};
		Registry.RegisterTool(MoveTemp(Reg));
	};

	RegisterSnapshotTool(SnapshotBuild::ToolName, SnapshotBuild::ToolDescription, SnapshotBuild::SchemaDefName, SnapshotBuild::ExecuteOnGameThread);
	RegisterSnapshotTool(SnapshotList::ToolName, SnapshotList::ToolDescription, SnapshotList::SchemaDefName, SnapshotList::ExecuteOnGameThread);
	RegisterSnapshotTool(SnapshotQuery::ToolName, SnapshotQuery::ToolDescription, SnapshotQuery::SchemaDefName, SnapshotQuery::ExecuteOnGameThread);
}
