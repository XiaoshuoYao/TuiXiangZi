// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_Assets.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "MCPToolRegistry.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/PackageName.h"

namespace
{
	// Helper: Build a minimal asset item JSON
	TSharedPtr<FJsonObject> MakeAssetItem(const FAssetData& AssetData, bool bIncludeTags = false)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("asset_path"), AssetData.GetObjectPathString());
		Item->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.ToString());
		Item->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());

		if (bIncludeTags)
		{
			TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
			AssetData.TagsAndValues.ForEach([&TagsObj](TPair<FName, FAssetTagValueRef> Pair)
			{
				TagsObj->SetStringField(Pair.Key.ToString(), Pair.Value.AsString());
				return true;
			});
			Item->SetObjectField(TEXT("tags"), TagsObj);
		}
		return Item;
	}
}

// ============================================================================
// ue_asset_search
// ============================================================================
namespace AssetSearch
{
	static constexpr const TCHAR* ToolName = TEXT("ue_asset_search");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Search assets by path prefix, class, text match, or tags. Supports pagination.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_asset_search");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString PathPrefix = MCPToolExecution::GetStringParam(Args, TEXT("path_prefix"));
		FString ClassName = MCPToolExecution::GetStringParam(Args, TEXT("class"));
		FString Text = MCPToolExecution::GetStringParam(Args, TEXT("text"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 50);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), false);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.assets.search") }, {}, true);

		if (Limit < 1)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument, TEXT("limit must be >= 1"));
			Envelope->SetArrayField(TEXT("items"), {});
			return Envelope;
		}
		if (Offset < 0)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument, TEXT("offset must be >= 0"));
			Envelope->SetArrayField(TEXT("items"), {});
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		if (!PathPrefix.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*PathPrefix));
			Filter.bRecursivePaths = true;
		}
		if (!ClassName.IsEmpty())
		{
			// FTopLevelAssetPath requires "/Script/Module.Class" format.
			// If the user passes a short name, resolve it via UClass::TryFindTypeSlow.
			if (ClassName.Contains(TEXT(".")))
			{
				Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
			}
			else
			{
				UClass* FoundClass = UClass::TryFindTypeSlow<UClass>(ClassName);
				if (FoundClass)
				{
					Filter.ClassPaths.Add(FoundClass->GetClassPathName());
				}
				else
				{
					MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
						FString::Printf(TEXT("Unknown class name '%s'. Use full path like '/Script/Engine.StaticMesh' or a valid short name."), *ClassName));
					Envelope->SetArrayField(TEXT("items"), {});
					return Envelope;
				}
			}
		}

		TArray<FAssetData> AllAssets;
		AssetRegistry.GetAssets(Filter, AllAssets);

		// Text secondary filter
		if (!Text.IsEmpty())
		{
			AllAssets.RemoveAll([&Text](const FAssetData& Asset)
			{
				return !Asset.AssetName.ToString().Contains(Text);
			});
		}

		// Sort for stable output
		AllAssets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});

		// Clamp to max_items
		int32 EffectiveLimit = FMath::Min(Limit, Snap.MaxItems);

		// Paginate
		int32 Total = AllAssets.Num();
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + EffectiveLimit, Total);

		TArray<TSharedPtr<FJsonValue>> Items;
		for (int32 i = Start; i < End; ++i)
		{
			Items.Add(MakeShared<FJsonValueObject>(MakeAssetItem(AllAssets[i])));
		}

		Envelope->SetArrayField(TEXT("items"), Items);
		Envelope->SetNumberField(TEXT("total"), Total);
		Envelope->SetNumberField(TEXT("offset"), Start);
		Envelope->SetNumberField(TEXT("limit"), EffectiveLimit);

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_asset_get
// ============================================================================
namespace AssetGet
{
	static constexpr const TCHAR* ToolName = TEXT("ue_asset_get");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get detailed info for a single asset by path.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_asset_get");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		bool bIncludeTags = MCPToolExecution::GetBoolParam(Args, TEXT("include_tags"), true);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.assets.get") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: asset_path"));
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
		if (!AssetData.IsValid())
		{
			// Try as package name fallback
			TArray<FAssetData> ByPackage;
			AssetRegistry.GetAssetsByPackageName(FName(*AssetPath), ByPackage);
			if (ByPackage.Num() > 0)
			{
				AssetData = ByPackage[0];
			}
		}

		if (!AssetData.IsValid())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
			return Envelope;
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("asset_path"), AssetData.GetObjectPathString());
		AssetObj->SetStringField(TEXT("asset_class"), AssetData.AssetClassPath.ToString());
		AssetObj->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
		AssetObj->SetBoolField(TEXT("exists_on_disk"), FPackageName::DoesPackageExist(AssetData.PackageName.ToString()));
		AssetObj->SetBoolField(TEXT("is_redirector"), AssetData.IsRedirector());

		if (bIncludeTags)
		{
			TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
			AssetData.TagsAndValues.ForEach([&TagsObj](TPair<FName, FAssetTagValueRef> Pair)
			{
				TagsObj->SetStringField(Pair.Key.ToString(), Pair.Value.AsString());
				return true;
			});
			AssetObj->SetObjectField(TEXT("tags"), TagsObj);
		}

		Envelope->SetObjectField(TEXT("asset"), AssetObj);
		return Envelope;
	}
}

// ============================================================================
// ue_asset_dependencies
// ============================================================================
namespace AssetDependencies
{
	static constexpr const TCHAR* ToolName = TEXT("ue_asset_dependencies");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get dependency graph (BFS) for an asset. Returns nodes and edges.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_asset_dependencies");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		FString Direction = MCPToolExecution::GetStringParam(Args, TEXT("direction"), TEXT("out"));
		int32 Depth = MCPToolExecution::GetIntParam(Args, TEXT("depth"), 1);
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 500);
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), true);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.assets.dependencies") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: asset_path"));
			Envelope->SetArrayField(TEXT("nodes"), {});
			Envelope->SetArrayField(TEXT("edges"), {});
			return Envelope;
		}
		if (Depth < 1)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument, TEXT("depth must be >= 1"));
			Envelope->SetArrayField(TEXT("nodes"), {});
			Envelope->SetArrayField(TEXT("edges"), {});
			return Envelope;
		}
		if (Limit < 1)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument, TEXT("limit must be >= 1"));
			Envelope->SetArrayField(TEXT("nodes"), {});
			Envelope->SetArrayField(TEXT("edges"), {});
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		// Verify starting asset exists
		FName StartPackage = FName(*FPackageName::ObjectPathToPackageName(AssetPath));

		// BFS
		TSet<FName> Visited;
		TArray<FName> CurrentLevel;
		CurrentLevel.Add(StartPackage);
		Visited.Add(StartPackage);

		TArray<TSharedPtr<FJsonValue>> Nodes;
		TArray<TSharedPtr<FJsonValue>> Edges;

		// Add root node
		{
			TSharedPtr<FJsonObject> RootNode = MakeShared<FJsonObject>();
			RootNode->SetStringField(TEXT("package"), StartPackage.ToString());
			RootNode->SetNumberField(TEXT("depth"), 0);
			Nodes.Add(MakeShared<FJsonValueObject>(RootNode));
		}

		bool bOut = (Direction == TEXT("out"));

		for (int32 d = 0; d < Depth && Nodes.Num() < Limit; ++d)
		{
			TArray<FName> NextLevel;

			for (const FName& Pkg : CurrentLevel)
			{
				if (Nodes.Num() >= Limit) break;

				TArray<FName> DepNames;
				if (bOut)
				{
					AssetRegistry.GetDependencies(Pkg, DepNames,
						UE::AssetRegistry::EDependencyCategory::Package);
				}
				else
				{
					AssetRegistry.GetReferencers(Pkg, DepNames,
						UE::AssetRegistry::EDependencyCategory::Package);
				}

				for (const FName& DepName : DepNames)
				{
					if (Nodes.Num() >= Limit) break;

					// Add edge
					TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
					if (bOut)
					{
						Edge->SetStringField(TEXT("from"), Pkg.ToString());
						Edge->SetStringField(TEXT("to"), DepName.ToString());
					}
					else
					{
						Edge->SetStringField(TEXT("from"), DepName.ToString());
						Edge->SetStringField(TEXT("to"), Pkg.ToString());
					}
					Edge->SetStringField(TEXT("kind"), TEXT("package"));
					Edges.Add(MakeShared<FJsonValueObject>(Edge));

					// Add node if new
					if (!Visited.Contains(DepName))
					{
						Visited.Add(DepName);
						NextLevel.Add(DepName);

						TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
						NodeObj->SetStringField(TEXT("package"), DepName.ToString());
						NodeObj->SetNumberField(TEXT("depth"), d + 1);
						Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
					}
				}
			}

			CurrentLevel = MoveTemp(NextLevel);
		}

		Envelope->SetArrayField(TEXT("nodes"), Nodes);
		Envelope->SetArrayField(TEXT("edges"), Edges);

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_asset_referencers
// ============================================================================
namespace AssetReferencers
{
	static constexpr const TCHAR* ToolName = TEXT("ue_asset_referencers");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get assets that reference (depend on) a given asset.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_asset_referencers");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 200);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.assets.referencers") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: asset_path"));
			Envelope->SetArrayField(TEXT("items"), {});
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FName PackageName = FName(*FPackageName::ObjectPathToPackageName(AssetPath));

		TArray<FName> ReferencerNames;
		AssetRegistry.GetReferencers(PackageName, ReferencerNames,
			UE::AssetRegistry::EDependencyCategory::Package);

		// Get asset data for each referencer
		TArray<TPair<FString, FString>> ReferencerItems; // (path, class)
		for (const FName& RefPkg : ReferencerNames)
		{
			TArray<FAssetData> PkgAssets;
			AssetRegistry.GetAssetsByPackageName(RefPkg, PkgAssets);
			for (const FAssetData& AD : PkgAssets)
			{
				ReferencerItems.Add(TPair<FString, FString>(
					AD.GetObjectPathString(),
					AD.AssetClassPath.ToString()));
			}
		}

		// Sort for stable output
		ReferencerItems.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });

		// Paginate
		int32 Total = ReferencerItems.Num();
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + FMath::Min(Limit, Snap.MaxItems), Total);

		TArray<TSharedPtr<FJsonValue>> Items;
		for (int32 i = Start; i < End; ++i)
		{
			TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("asset_path"), ReferencerItems[i].Key);
			Item->SetStringField(TEXT("asset_class"), ReferencerItems[i].Value);
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}

		Envelope->SetArrayField(TEXT("items"), Items);
		Envelope->SetNumberField(TEXT("total"), Total);
		return Envelope;
	}
}

// ============================================================================
// ue_asset_tags_index
// ============================================================================
namespace AssetTagsIndex
{
	static constexpr const TCHAR* ToolName = TEXT("ue_asset_tags_index");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get all unique tag keys used by assets matching a filter.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_asset_tags_index");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString PathPrefix = MCPToolExecution::GetStringParam(Args, TEXT("path_prefix"));
		FString ClassName = MCPToolExecution::GetStringParam(Args, TEXT("class"));

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.assets.tags_index") }, {}, true);

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		if (!PathPrefix.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*PathPrefix));
			Filter.bRecursivePaths = true;
		}
		if (!ClassName.IsEmpty())
		{
			if (ClassName.Contains(TEXT(".")))
			{
				Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
			}
			else
			{
				UClass* FoundClass = UClass::TryFindTypeSlow<UClass>(ClassName);
				if (FoundClass)
				{
					Filter.ClassPaths.Add(FoundClass->GetClassPathName());
				}
				else
				{
					MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
						FString::Printf(TEXT("Unknown class name '%s'. Use full path like '/Script/Engine.StaticMesh' or a valid short name."), *ClassName));
					Envelope->SetArrayField(TEXT("items"), {});
					return Envelope;
				}
			}
		}

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		TSet<FName> TagKeys;
		for (const FAssetData& AD : Assets)
		{
			AD.TagsAndValues.ForEach([&TagKeys](TPair<FName, FAssetTagValueRef> Pair)
			{
				TagKeys.Add(Pair.Key);
				return true;
			});
		}

		TArray<FString> SortedKeys;
		for (const FName& Key : TagKeys)
		{
			SortedKeys.Add(Key.ToString());
		}
		SortedKeys.Sort();

		TArray<TSharedPtr<FJsonValue>> KeysArray;
		for (const FString& Key : SortedKeys)
		{
			KeysArray.Add(MakeShared<FJsonValueString>(Key));
		}
		Envelope->SetArrayField(TEXT("tag_keys"), KeysArray);

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_Assets::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
{
	// Helper macro-like lambda for registering GameThread tools
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

	RegisterGameThreadTool(AssetSearch::ToolName, AssetSearch::ToolDescription, AssetSearch::SchemaDefName, AssetSearch::ExecuteOnGameThread);
	RegisterGameThreadTool(AssetGet::ToolName, AssetGet::ToolDescription, AssetGet::SchemaDefName, AssetGet::ExecuteOnGameThread);
	RegisterGameThreadTool(AssetDependencies::ToolName, AssetDependencies::ToolDescription, AssetDependencies::SchemaDefName, AssetDependencies::ExecuteOnGameThread);
	RegisterGameThreadTool(AssetReferencers::ToolName, AssetReferencers::ToolDescription, AssetReferencers::SchemaDefName, AssetReferencers::ExecuteOnGameThread);
	RegisterGameThreadTool(AssetTagsIndex::ToolName, AssetTagsIndex::ToolDescription, AssetTagsIndex::SchemaDefName, AssetTagsIndex::ExecuteOnGameThread);
}
