// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_Materials.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "MCPToolRegistry.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

#include "Engine/Texture.h"

// ============================================================================
// ue_mat_params_get
// ============================================================================
namespace MatParamsGet
{
	static constexpr const TCHAR* ToolName = TEXT("ue_mat_params_get");
	static constexpr const TCHAR* ToolDescription =
		TEXT("List material/material instance parameters with types, values, and override status.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_mat_params_get");

	// Helper: check if a scalar param is overridden in a MaterialInstanceConstant
	bool IsScalarOverridden(const UMaterialInstanceConstant* MIC, FName ParamName)
	{
		for (const FScalarParameterValue& SPV : MIC->ScalarParameterValues)
		{
			if (SPV.ParameterInfo.Name == ParamName)
			{
				return true;
			}
		}
		return false;
	}

	bool IsVectorOverridden(const UMaterialInstanceConstant* MIC, FName ParamName)
	{
		for (const FVectorParameterValue& VPV : MIC->VectorParameterValues)
		{
			if (VPV.ParameterInfo.Name == ParamName)
			{
				return true;
			}
		}
		return false;
	}

	bool IsTextureOverridden(const UMaterialInstanceConstant* MIC, FName ParamName)
	{
		for (const FTextureParameterValue& TPV : MIC->TextureParameterValues)
		{
			if (TPV.ParameterInfo.Name == ParamName)
			{
				return true;
			}
		}
		return false;
	}

	bool IsStaticSwitchOverridden(const UMaterialInstanceConstant* MIC, FName ParamName)
	{
#if WITH_EDITOR
		FStaticParameterSet StaticParams;
		const_cast<UMaterialInstanceConstant*>(MIC)->GetStaticParameterValues(StaticParams);
		for (const FStaticSwitchParameter& SSP : StaticParams.StaticSwitchParameters)
		{
			if (SSP.ParameterInfo.Name == ParamName)
			{
				return SSP.bOverride;
			}
		}
#endif
		return false;
	}

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString MaterialPath = MCPToolExecution::GetStringParam(Args, TEXT("material_path"));
		bool bIncludeOverrides = MCPToolExecution::GetBoolParam(Args, TEXT("include_overrides"), true);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.materials.params") }, {}, true);

		if (MaterialPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: material_path"));
			Envelope->SetArrayField(TEXT("params"), {});
			return Envelope;
		}

		// Load material interface
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Mat)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
			Envelope->SetArrayField(TEXT("params"), {});
			return Envelope;
		}

		// Determine type
		UMaterial* BaseMat = Cast<UMaterial>(Mat);
		UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Mat);
		UMaterialInstance* MatInst = Cast<UMaterialInstance>(Mat);

		Envelope->SetStringField(TEXT("material_path"), Mat->GetPathName());

		if (BaseMat)
		{
			Envelope->SetStringField(TEXT("material_class"), TEXT("Material"));
			Envelope->SetField(TEXT("parent_material"), MakeShared<FJsonValueNull>());
		}
		else if (MIC)
		{
			Envelope->SetStringField(TEXT("material_class"), TEXT("MaterialInstanceConstant"));
			if (MIC->Parent)
			{
				Envelope->SetStringField(TEXT("parent_material"), MIC->Parent->GetPathName());
			}
			else
			{
				Envelope->SetField(TEXT("parent_material"), MakeShared<FJsonValueNull>());
			}
		}
		else if (MatInst)
		{
			Envelope->SetStringField(TEXT("material_class"), TEXT("MaterialInstance"));
			if (MatInst->Parent)
			{
				Envelope->SetStringField(TEXT("parent_material"), MatInst->Parent->GetPathName());
			}
			else
			{
				Envelope->SetField(TEXT("parent_material"), MakeShared<FJsonValueNull>());
			}
		}

		TArray<TSharedPtr<FJsonValue>> ParamsArray;

		// --- Scalar Parameters ---
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

				if (bIncludeOverrides && MIC)
				{
					Param->SetBoolField(TEXT("overridden"), IsScalarOverridden(MIC, Info.Name));
				}

				ParamsArray.Add(MakeShared<FJsonValueObject>(Param));
			}
		}

		// --- Vector Parameters ---
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

				if (bIncludeOverrides && MIC)
				{
					Param->SetBoolField(TEXT("overridden"), IsVectorOverridden(MIC, Info.Name));
				}

				ParamsArray.Add(MakeShared<FJsonValueObject>(Param));
			}
		}

		// --- Texture Parameters ---
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

				if (bIncludeOverrides && MIC)
				{
					Param->SetBoolField(TEXT("overridden"), IsTextureOverridden(MIC, Info.Name));
				}

				ParamsArray.Add(MakeShared<FJsonValueObject>(Param));
			}
		}

		// --- Static Switch Parameters ---
#if WITH_EDITOR
		{
			TArray<FMaterialParameterInfo> SwitchInfos;
			TArray<FGuid> SwitchGuids;
			Mat->GetAllStaticSwitchParameterInfo(SwitchInfos, SwitchGuids);

			for (const FMaterialParameterInfo& Info : SwitchInfos)
			{
				bool SwitchValue = false;
				FGuid OutGuid;
				Mat->GetStaticSwitchParameterValue(Info.Name, SwitchValue, OutGuid);

				TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
				Param->SetStringField(TEXT("name"), Info.Name.ToString());
				Param->SetStringField(TEXT("type"), TEXT("static_switch"));
				Param->SetBoolField(TEXT("value"), SwitchValue);

				if (bIncludeOverrides && MIC)
				{
					Param->SetBoolField(TEXT("overridden"), IsStaticSwitchOverridden(MIC, Info.Name));
				}

				ParamsArray.Add(MakeShared<FJsonValueObject>(Param));
			}
		}
#endif

		Envelope->SetArrayField(TEXT("params"), ParamsArray);
		return Envelope;
	}
}

// ============================================================================
// ue_mat_graph_get
// ============================================================================
namespace MatGraphGet
{
	static constexpr const TCHAR* ToolName = TEXT("ue_mat_graph_get");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Export material expression graph as JSON GraphData. Only works on UMaterial (not instances).");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_mat_graph_get");

	// Get a stable string ID for an expression
	FString GetExpressionId(const UMaterialExpression* Expression)
	{
		if (Expression->MaterialExpressionGuid.IsValid())
		{
			return Expression->MaterialExpressionGuid.ToString();
		}
		return FString::Printf(TEXT("expr_%s_%p"), *Expression->GetClass()->GetName(),
			static_cast<const void*>(Expression));
	}

	// Get a human-readable title for an expression
	FString GetExpressionTitle(const UMaterialExpression* Expression)
	{
		// Parameter nodes: use parameter name
		if (const UMaterialExpressionScalarParameter* SP = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			return SP->ParameterName.ToString();
		}
		if (const UMaterialExpressionVectorParameter* VP = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			return VP->ParameterName.ToString();
		}
		if (const UMaterialExpressionTextureSampleParameter2D* TP = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
		{
			return TP->ParameterName.ToString();
		}
		if (const UMaterialExpressionStaticSwitchParameter* SSP = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			return SSP->ParameterName.ToString();
		}

		// Constant nodes: show value
		if (const UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(Expression))
		{
			return FString::Printf(TEXT("%.4f"), C->R);
		}
		if (const UMaterialExpressionConstant3Vector* C3 = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			return FString::Printf(TEXT("(%.2f, %.2f, %.2f, %.2f)"), C3->Constant.R, C3->Constant.G, C3->Constant.B, C3->Constant.A);
		}

		// Fallback: class name without "MaterialExpression" prefix
		FString ClassName = Expression->GetClass()->GetName();
		ClassName.RemoveFromStart(TEXT("MaterialExpression"));
		return ClassName;
	}

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString MaterialPath = MCPToolExecution::GetStringParam(Args, TEXT("material_path"));
		FString Detail = MCPToolExecution::GetStringParam(Args, TEXT("detail"), TEXT("compact"));
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), true);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.materials.graph") }, {}, true);

		if (MaterialPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: material_path"));
			return Envelope;
		}

		// Load as UMaterial (not MI)
		UMaterialInterface* MatInterface = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!MatInterface)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
			return Envelope;
		}

		UMaterial* Material = Cast<UMaterial>(MatInterface);
		if (!Material)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("ue_mat_graph_get only works on UMaterial, not MaterialInstance. Use the parent material path instead."));
			return Envelope;
		}

		Envelope->SetStringField(TEXT("material_path"), Material->GetPathName());

		bool bFull = (Detail == TEXT("full"));

		// Build a map from expression pointer to ID for edge generation
		TMap<const UMaterialExpression*, FString> ExprIdMap;

#if WITH_EDITORONLY_DATA
		const TArray<TObjectPtr<UMaterialExpression>>& Expressions = Material->GetExpressionCollection().Expressions;

		// First pass: assign IDs
		for (const TObjectPtr<UMaterialExpression>& ExprPtr : Expressions)
		{
			if (UMaterialExpression* Expr = ExprPtr.Get())
			{
				ExprIdMap.Add(Expr, GetExpressionId(Expr));
			}
		}

		// Build nodes and edges
		TArray<TSharedPtr<FJsonValue>> NodesArray;
		TArray<TSharedPtr<FJsonValue>> EdgesArray;

		for (const TObjectPtr<UMaterialExpression>& ExprPtr : Expressions)
		{
			UMaterialExpression* Expr = ExprPtr.Get();
			if (!Expr) continue;

			// Skip comment nodes in compact mode
			if (!bFull && Expr->IsA(UMaterialExpressionComment::StaticClass()))
			{
				continue;
			}

			FString ExprId = ExprIdMap[Expr];

			// --- Build node ---
			TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
			Node->SetStringField(TEXT("id"), ExprId);

			FString Kind = Expr->GetClass()->GetName();
			Node->SetStringField(TEXT("kind"), Kind);
			Node->SetStringField(TEXT("title"), GetExpressionTitle(Expr));

			// Position
			TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
			PosObj->SetNumberField(TEXT("x"), Expr->MaterialExpressionEditorX);
			PosObj->SetNumberField(TEXT("y"), Expr->MaterialExpressionEditorY);
			Node->SetObjectField(TEXT("pos"), PosObj);

			// --- Build pins ---
			TSharedPtr<FJsonObject> PinsObj = MakeShared<FJsonObject>();

			// Input pins
			TArray<TSharedPtr<FJsonValue>> InputPins;
			for (FExpressionInputIterator It(Expr); It; ++It)
			{
				FName InputName = Expr->GetInputName(It.Index);
				FString PinName = InputName.IsNone() ? FString::Printf(TEXT("%d"), It.Index) : InputName.ToString();
				FString PinId = FString::Printf(TEXT("%s:in:%s"), *ExprId, *PinName);

				TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
				Pin->SetStringField(TEXT("id"), PinId);
				Pin->SetStringField(TEXT("name"), PinName);
				InputPins.Add(MakeShared<FJsonValueObject>(Pin));

				// --- Build edge if connected ---
				FExpressionInput* Input = It.Input;
				if (Input && Input->Expression)
				{
					const FString* FromExprId = ExprIdMap.Find(Input->Expression);
					if (FromExprId)
					{
						FString FromPinId = FString::Printf(TEXT("%s:out:%d"), **FromExprId, Input->OutputIndex);

						TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
						Edge->SetStringField(TEXT("from_pin"), FromPinId);
						Edge->SetStringField(TEXT("to_pin"), PinId);
						EdgesArray.Add(MakeShared<FJsonValueObject>(Edge));
					}
				}
			}
			PinsObj->SetArrayField(TEXT("inputs"), InputPins);

			// Output pins
			TArray<TSharedPtr<FJsonValue>> OutputPins;
			TArray<FExpressionOutput>& Outputs = Expr->GetOutputs();
			for (int32 i = 0; i < Outputs.Num(); ++i)
			{
				FString OutName = Outputs[i].OutputName.IsNone()
					? FString::Printf(TEXT("%d"), i)
					: Outputs[i].OutputName.ToString();
				FString PinId = FString::Printf(TEXT("%s:out:%d"), *ExprId, i);

				TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
				Pin->SetStringField(TEXT("id"), PinId);
				Pin->SetStringField(TEXT("name"), OutName);
				OutputPins.Add(MakeShared<FJsonValueObject>(Pin));
			}
			PinsObj->SetArrayField(TEXT("outputs"), OutputPins);

			Node->SetObjectField(TEXT("pins"), PinsObj);

			// --- Full detail: extra attributes ---
			if (bFull)
			{
				TSharedPtr<FJsonObject> Attrs = MakeShared<FJsonObject>();
				bool bHasAttrs = false;

				if (const UMaterialExpressionScalarParameter* SP = Cast<UMaterialExpressionScalarParameter>(Expr))
				{
					Attrs->SetNumberField(TEXT("default_value"), SP->DefaultValue);
					Attrs->SetStringField(TEXT("group"), SP->Group.ToString());
					Attrs->SetNumberField(TEXT("sort_priority"), SP->SortPriority);
					bHasAttrs = true;
				}
				else if (const UMaterialExpressionVectorParameter* VP = Cast<UMaterialExpressionVectorParameter>(Expr))
				{
					TSharedPtr<FJsonObject> DefVal = MakeShared<FJsonObject>();
					DefVal->SetNumberField(TEXT("r"), VP->DefaultValue.R);
					DefVal->SetNumberField(TEXT("g"), VP->DefaultValue.G);
					DefVal->SetNumberField(TEXT("b"), VP->DefaultValue.B);
					DefVal->SetNumberField(TEXT("a"), VP->DefaultValue.A);
					Attrs->SetObjectField(TEXT("default_value"), DefVal);
					Attrs->SetStringField(TEXT("group"), VP->Group.ToString());
					bHasAttrs = true;
				}
				else if (const UMaterialExpressionTextureSample* TS = Cast<UMaterialExpressionTextureSample>(Expr))
				{
					if (TS->Texture)
					{
						Attrs->SetStringField(TEXT("texture"), TS->Texture->GetPathName());
					}
					bHasAttrs = true;
				}
				else if (const UMaterialExpressionConstant* C = Cast<UMaterialExpressionConstant>(Expr))
				{
					Attrs->SetNumberField(TEXT("value"), C->R);
					bHasAttrs = true;
				}
				else if (const UMaterialExpressionConstant3Vector* C3 = Cast<UMaterialExpressionConstant3Vector>(Expr))
				{
					TSharedPtr<FJsonObject> Val = MakeShared<FJsonObject>();
					Val->SetNumberField(TEXT("r"), C3->Constant.R);
					Val->SetNumberField(TEXT("g"), C3->Constant.G);
					Val->SetNumberField(TEXT("b"), C3->Constant.B);
					Val->SetNumberField(TEXT("a"), C3->Constant.A);
					Attrs->SetObjectField(TEXT("value"), Val);
					bHasAttrs = true;
				}

				if (bHasAttrs)
				{
					Node->SetObjectField(TEXT("attributes"), Attrs);
				}

				// Comment / description
				if (!Expr->Desc.IsEmpty())
				{
					Node->SetStringField(TEXT("comment"), Expr->Desc);
				}
			}

			NodesArray.Add(MakeShared<FJsonValueObject>(Node));
		}

		// --- Material attributes (connected output pins) ---
		TArray<TSharedPtr<FJsonValue>> MaterialAttributes;

		// In UE 5.7, material inputs are on UMaterialEditorOnlyData
		UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
		if (EditorData)
		{
			auto CheckInput = [&](const FExpressionInput& Input, const TCHAR* Name)
			{
				if (Input.Expression)
				{
					MaterialAttributes.Add(MakeShared<FJsonValueString>(Name));

					const FString* FromId = ExprIdMap.Find(Input.Expression);
					if (FromId)
					{
						FString FromPinId = FString::Printf(TEXT("%s:out:%d"), **FromId, Input.OutputIndex);
						FString ToPinId = FString::Printf(TEXT("material:%s"), Name);

						TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
						Edge->SetStringField(TEXT("from_pin"), FromPinId);
						Edge->SetStringField(TEXT("to_pin"), ToPinId);
						EdgesArray.Add(MakeShared<FJsonValueObject>(Edge));
					}
				}
			};

			CheckInput(EditorData->BaseColor,      TEXT("BaseColor"));
			CheckInput(EditorData->Metallic,        TEXT("Metallic"));
			CheckInput(EditorData->Specular,        TEXT("Specular"));
			CheckInput(EditorData->Roughness,       TEXT("Roughness"));
			CheckInput(EditorData->Anisotropy,      TEXT("Anisotropy"));
			CheckInput(EditorData->Normal,           TEXT("Normal"));
			CheckInput(EditorData->Tangent,          TEXT("Tangent"));
			CheckInput(EditorData->EmissiveColor,    TEXT("EmissiveColor"));
			CheckInput(EditorData->Opacity,          TEXT("Opacity"));
			CheckInput(EditorData->OpacityMask,      TEXT("OpacityMask"));
			CheckInput(EditorData->WorldPositionOffset, TEXT("WorldPositionOffset"));
			CheckInput(EditorData->AmbientOcclusion, TEXT("AmbientOcclusion"));
		}

		// Assemble graph
		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetArrayField(TEXT("nodes"), NodesArray);
		Graph->SetArrayField(TEXT("edges"), EdgesArray);
		Graph->SetArrayField(TEXT("material_attributes"), MaterialAttributes);

		Envelope->SetObjectField(TEXT("graph"), Graph);

#else
		// No editor-only data available
		MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("Material graph data requires WITH_EDITORONLY_DATA (Editor build)"));
#endif

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_mat_find_param_usage
// ============================================================================
namespace MatFindParamUsage
{
	static constexpr const TCHAR* ToolName = TEXT("ue_mat_find_param_usage");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Find which materials and material instances use a given parameter name across the project.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_mat_find_param_usage");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString ParamName = MCPToolExecution::GetStringParam(Args, TEXT("param_name"));
		FString PathPrefix = MCPToolExecution::GetStringParam(Args, TEXT("path_prefix"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 50);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.materials.find_param") }, {}, true);

		if (ParamName.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: param_name"));
			Envelope->SetArrayField(TEXT("hits"), {});
			return Envelope;
		}

		if (PathPrefix.IsEmpty())
		{
			Envelope->Values.FindOrAdd(TEXT("warnings"));
			TArray<TSharedPtr<FJsonValue>> Warnings;
			Warnings.Add(MakeShared<FJsonValueString>(TEXT("No path_prefix specified. Scanning entire project may be slow.")));
			Envelope->SetArrayField(TEXT("warnings"), Warnings);
		}

		FName SearchName = FName(*ParamName);

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		// Collect all hits
		struct FParamHit
		{
			FString MaterialPath;
			FString Kind; // "base" or "instance"
			FString ParamType;
			bool bOverridden = false;
			TSharedPtr<FJsonValue> Value;
		};

		TArray<FParamHit> AllHits;

		// --- Search UMaterial assets ---
#if WITH_EDITORONLY_DATA
		{
			FARFilter MatFilter;
			MatFilter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
			if (!PathPrefix.IsEmpty())
			{
				MatFilter.PackagePaths.Add(FName(*PathPrefix));
				MatFilter.bRecursivePaths = true;
			}

			TArray<FAssetData> MatAssets;
			AssetRegistry.GetAssets(MatFilter, MatAssets);

			for (const FAssetData& AD : MatAssets)
			{
				UMaterial* Mat = Cast<UMaterial>(AD.GetAsset());
				if (!Mat) continue;

				for (UMaterialExpression* Expr : Mat->GetExpressionCollection().Expressions)
				{
					if (!Expr) continue;

					if (UMaterialExpressionScalarParameter* SP = Cast<UMaterialExpressionScalarParameter>(Expr))
					{
						if (SP->ParameterName == SearchName)
						{
							FParamHit Hit;
							Hit.MaterialPath = AD.GetObjectPathString();
							Hit.Kind = TEXT("base");
							Hit.ParamType = TEXT("scalar");
							Hit.Value = MakeShared<FJsonValueNumber>(SP->DefaultValue);
							AllHits.Add(MoveTemp(Hit));
						}
					}
					else if (UMaterialExpressionVectorParameter* VP = Cast<UMaterialExpressionVectorParameter>(Expr))
					{
						if (VP->ParameterName == SearchName)
						{
							TSharedPtr<FJsonObject> Val = MakeShared<FJsonObject>();
							Val->SetNumberField(TEXT("r"), VP->DefaultValue.R);
							Val->SetNumberField(TEXT("g"), VP->DefaultValue.G);
							Val->SetNumberField(TEXT("b"), VP->DefaultValue.B);
							Val->SetNumberField(TEXT("a"), VP->DefaultValue.A);

							FParamHit Hit;
							Hit.MaterialPath = AD.GetObjectPathString();
							Hit.Kind = TEXT("base");
							Hit.ParamType = TEXT("vector");
							Hit.Value = MakeShared<FJsonValueObject>(Val);
							AllHits.Add(MoveTemp(Hit));
						}
					}
					else if (UMaterialExpressionTextureSampleParameter2D* TP = Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
					{
						if (TP->ParameterName == SearchName)
						{
							FParamHit Hit;
							Hit.MaterialPath = AD.GetObjectPathString();
							Hit.Kind = TEXT("base");
							Hit.ParamType = TEXT("texture");
							Hit.Value = TP->Texture
								? TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(TP->Texture->GetPathName()))
								: TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>());
							AllHits.Add(MoveTemp(Hit));
						}
					}
					else if (UMaterialExpressionStaticSwitchParameter* SSP = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
					{
						if (SSP->ParameterName == SearchName)
						{
							FParamHit Hit;
							Hit.MaterialPath = AD.GetObjectPathString();
							Hit.Kind = TEXT("base");
							Hit.ParamType = TEXT("static_switch");
							Hit.Value = MakeShared<FJsonValueBoolean>(static_cast<bool>(SSP->DefaultValue));
							AllHits.Add(MoveTemp(Hit));
						}
					}
				}
			}
		}
#endif

		// --- Search MaterialInstanceConstant assets ---
		{
			FARFilter MICFilter;
			MICFilter.ClassPaths.Add(UMaterialInstanceConstant::StaticClass()->GetClassPathName());
			if (!PathPrefix.IsEmpty())
			{
				MICFilter.PackagePaths.Add(FName(*PathPrefix));
				MICFilter.bRecursivePaths = true;
			}

			TArray<FAssetData> MICAssets;
			AssetRegistry.GetAssets(MICFilter, MICAssets);

			for (const FAssetData& AD : MICAssets)
			{
				UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(AD.GetAsset());
				if (!MIC) continue;

				// Check scalar overrides
				for (const FScalarParameterValue& SPV : MIC->ScalarParameterValues)
				{
					if (SPV.ParameterInfo.Name == SearchName)
					{
						FParamHit Hit;
						Hit.MaterialPath = AD.GetObjectPathString();
						Hit.Kind = TEXT("instance");
						Hit.ParamType = TEXT("scalar");
						Hit.bOverridden = true;
						Hit.Value = MakeShared<FJsonValueNumber>(SPV.ParameterValue);
						AllHits.Add(MoveTemp(Hit));
					}
				}

				// Check vector overrides
				for (const FVectorParameterValue& VPV : MIC->VectorParameterValues)
				{
					if (VPV.ParameterInfo.Name == SearchName)
					{
						TSharedPtr<FJsonObject> Val = MakeShared<FJsonObject>();
						Val->SetNumberField(TEXT("r"), VPV.ParameterValue.R);
						Val->SetNumberField(TEXT("g"), VPV.ParameterValue.G);
						Val->SetNumberField(TEXT("b"), VPV.ParameterValue.B);
						Val->SetNumberField(TEXT("a"), VPV.ParameterValue.A);

						FParamHit Hit;
						Hit.MaterialPath = AD.GetObjectPathString();
						Hit.Kind = TEXT("instance");
						Hit.ParamType = TEXT("vector");
						Hit.bOverridden = true;
						Hit.Value = MakeShared<FJsonValueObject>(Val);
						AllHits.Add(MoveTemp(Hit));
					}
				}

				// Check texture overrides
				for (const FTextureParameterValue& TPV : MIC->TextureParameterValues)
				{
					if (TPV.ParameterInfo.Name == SearchName)
					{
						FParamHit Hit;
						Hit.MaterialPath = AD.GetObjectPathString();
						Hit.Kind = TEXT("instance");
						Hit.ParamType = TEXT("texture");
						Hit.bOverridden = true;
						Hit.Value = TPV.ParameterValue
							? TSharedPtr<FJsonValue>(MakeShared<FJsonValueString>(TPV.ParameterValue->GetPathName()))
							: TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>());
						AllHits.Add(MoveTemp(Hit));
					}
				}

				// Check static switch overrides
#if WITH_EDITOR
				{
					FStaticParameterSet StaticParams;
					MIC->GetStaticParameterValues(StaticParams);
					for (const FStaticSwitchParameter& SSP : StaticParams.StaticSwitchParameters)
					{
						if (SSP.ParameterInfo.Name == SearchName && SSP.bOverride)
						{
							FParamHit Hit;
							Hit.MaterialPath = AD.GetObjectPathString();
							Hit.Kind = TEXT("instance");
							Hit.ParamType = TEXT("static_switch");
							Hit.bOverridden = true;
							Hit.Value = MakeShared<FJsonValueBoolean>(SSP.Value);
							AllHits.Add(MoveTemp(Hit));
						}
					}
				}
#endif
			}
		}

		// Sort: base materials first, then instances
		AllHits.Sort([](const FParamHit& A, const FParamHit& B)
		{
			if (A.Kind != B.Kind)
			{
				return A.Kind == TEXT("base"); // base before instance
			}
			return A.MaterialPath < B.MaterialPath;
		});

		// Pagination
		int32 Total = AllHits.Num();
		int32 EffectiveLimit = FMath::Min(Limit, Snap.MaxItems);
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + EffectiveLimit, Total);

		TArray<TSharedPtr<FJsonValue>> HitsArray;
		for (int32 i = Start; i < End; ++i)
		{
			const FParamHit& Hit = AllHits[i];

			TSharedPtr<FJsonObject> HitObj = MakeShared<FJsonObject>();
			HitObj->SetStringField(TEXT("material_path"), Hit.MaterialPath);
			HitObj->SetStringField(TEXT("kind"), Hit.Kind);
			HitObj->SetStringField(TEXT("param_type"), Hit.ParamType);

			if (Hit.Kind == TEXT("base"))
			{
				if (Hit.Value.IsValid())
				{
					HitObj->SetField(TEXT("default_value"), Hit.Value);
				}
			}
			else
			{
				HitObj->SetBoolField(TEXT("overridden"), Hit.bOverridden);
				if (Hit.Value.IsValid())
				{
					HitObj->SetField(TEXT("current_value"), Hit.Value);
				}
			}

			HitsArray.Add(MakeShared<FJsonValueObject>(HitObj));
		}

		Envelope->SetStringField(TEXT("param_name"), ParamName);
		Envelope->SetNumberField(TEXT("total"), Total);
		Envelope->SetNumberField(TEXT("offset"), Start);
		Envelope->SetNumberField(TEXT("limit"), EffectiveLimit);
		Envelope->SetArrayField(TEXT("hits"), HitsArray);

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_Materials::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
{
	auto RegisterGameThreadTool = [&](const TCHAR* Name, const TCHAR* Desc, const TCHAR* SchemaDef,
		TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&, FMCPRuntimeState&, FMCPResourceStore&)> Impl)
	{
		TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(SchemaDef);
		FMCPToolRegistration Reg;
		Reg.Descriptor.Name = Name;
		Reg.Descriptor.Description = Desc;
		Reg.Descriptor.InputSchema = Schema;
		Reg.Execute = [&RuntimeState, &ResourceStore, Impl](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
		{
			return MCPToolExecution::RunOnGameThread([&]()
			{
				return Impl(Args, RuntimeState, ResourceStore);
			});
		};
		Registry.RegisterTool(MoveTemp(Reg));
	};

	RegisterGameThreadTool(MatParamsGet::ToolName, MatParamsGet::ToolDescription, MatParamsGet::SchemaDefName, MatParamsGet::ExecuteOnGameThread);
	RegisterGameThreadTool(MatGraphGet::ToolName, MatGraphGet::ToolDescription, MatGraphGet::SchemaDefName, MatGraphGet::ExecuteOnGameThread);
	RegisterGameThreadTool(MatFindParamUsage::ToolName, MatFindParamUsage::ToolDescription, MatFindParamUsage::SchemaDefName, MatFindParamUsage::ExecuteOnGameThread);
}
