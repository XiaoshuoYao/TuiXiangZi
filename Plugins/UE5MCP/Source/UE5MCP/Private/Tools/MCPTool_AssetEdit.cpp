// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_AssetEdit.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "MCPToolRegistry.h"
#include "Edit/MCPAssetPathValidator.h"
#include "Edit/MCPEditErrors.h"
#include "Edit/MCPEditEnvelope.h"
#include "Reflection/MCPReflectionCore.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectGlobals.h"
#endif

// ============================================================================
// Helper: Read-only guard
// ============================================================================
namespace
{
	bool IsReadOnly(FMCPRuntimeState& RuntimeState, TSharedPtr<FJsonObject>& OutEnvelope)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();
		if (Snap.bReadOnly)
		{
			OutEnvelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(OutEnvelope, MCPToolExecution::ErrorReadOnly,
				TEXT("Server is in read-only mode. Write operations are disabled."));
			return true;
		}
		return false;
	}
}

// ============================================================================
// ue_create_folder
// ============================================================================
namespace CreateFolder
{
	static constexpr const TCHAR* ToolName = TEXT("ue_create_folder");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Create a content folder under /Game/. Creates intermediate directories as needed.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_create_folder");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString FolderPath = MCPToolExecution::GetStringParam(Args, TEXT("folder_path"));

		// Validate package path
		FString PathError = MCPAssetPathValidator::ValidatePackagePath(FolderPath);
		if (!PathError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath, PathError);
			return Envelope;
		}

		// Convert to disk path and create directory
		FString DiskPath;
		if (!FPackageName::TryConvertLongPackageNameToFilename(FolderPath, DiskPath))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath,
				FString::Printf(TEXT("Could not resolve disk path for: %s"), *FolderPath));
			return Envelope;
		}

		bool bCreated = IFileManager::Get().MakeDirectory(*DiskPath, true);

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);
		Envelope->SetBoolField(TEXT("created"), bCreated);
		Envelope->SetStringField(TEXT("folder_path"), FolderPath);
		return Envelope;
	}
}

// ============================================================================
// ue_create_asset
// ============================================================================
namespace CreateAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_create_asset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Create a new asset (BlueprintClass, DataAsset, or MaterialInstance). Supports dry_run mode.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_create_asset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetType = MCPToolExecution::GetStringParam(Args, TEXT("asset_type"));
		FString PackagePath = MCPToolExecution::GetStringParam(Args, TEXT("package_path"));
		FString AssetName = MCPToolExecution::GetStringParam(Args, TEXT("asset_name"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);

		// Extract init sub-object
		TSharedPtr<FJsonObject> InitObj;
		if (Args.IsValid() && Args->HasField(TEXT("init")))
		{
			InitObj = Args->GetObjectField(TEXT("init"));
		}

		// Validate
		FString PathError = MCPAssetPathValidator::ValidatePackagePath(PackagePath);
		if (!PathError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath, PathError);
			return Envelope;
		}

		FString NameError = MCPAssetPathValidator::ValidateAssetName(AssetName);
		if (!NameError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidAssetName, NameError);
			return Envelope;
		}

		if (MCPAssetPathValidator::DoesAssetExist(PackagePath, AssetName))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::NameConflict,
				FString::Printf(TEXT("Asset already exists: %s/%s"), *PackagePath, *AssetName));
			return Envelope;
		}

		// Validate asset_type
		if (AssetType != TEXT("BlueprintClass") && AssetType != TEXT("DataAsset") && AssetType != TEXT("MaterialInstance"))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::UnsupportedAssetType,
				FString::Printf(TEXT("Unsupported asset_type: %s. Supported: BlueprintClass, DataAsset, MaterialInstance"), *AssetType));
			return Envelope;
		}

		// Dry run: just validate
		if (bDryRun)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(true, true);
			Envelope->SetStringField(TEXT("asset_type"), AssetType);
			Envelope->SetStringField(TEXT("package_path"), PackagePath);
			Envelope->SetStringField(TEXT("asset_name"), AssetName);
			Envelope->SetStringField(TEXT("message"), TEXT("Validation passed. Asset would be created."));
			return Envelope;
		}

#if WITH_EDITOR
		FString FullPath = PackagePath / AssetName;
		UObject* CreatedAsset = nullptr;

		if (AssetType == TEXT("BlueprintClass"))
		{
			FString ParentClassName = TEXT("Actor");
			if (InitObj.IsValid() && InitObj->HasField(TEXT("parent_class")))
			{
				ParentClassName = InitObj->GetStringField(TEXT("parent_class"));
			}

			UClass* ParentClass = MCPReflection::ResolveClass(ParentClassName);
			if (!ParentClass)
			{
				Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
				MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidClassReference,
					FString::Printf(TEXT("Could not resolve parent class: %s"), *ParentClassName));
				return Envelope;
			}

			UPackage* Package = CreatePackage(*FullPath);
			if (!Package)
			{
				Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
				MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
					TEXT("Failed to create package"));
				return Envelope;
			}

			FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Create Blueprint")));

			UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
				ParentClass,
				Package,
				FName(*AssetName),
				BPTYPE_Normal,
				UBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass());

			if (NewBP)
			{
				FAssetRegistryModule::AssetCreated(NewBP);
				NewBP->MarkPackageDirty();
				CreatedAsset = NewBP;
			}
		}
		else if (AssetType == TEXT("DataAsset"))
		{
			FString AssetClassName = TEXT("UDataAsset");
			if (InitObj.IsValid() && InitObj->HasField(TEXT("asset_class")))
			{
				AssetClassName = InitObj->GetStringField(TEXT("asset_class"));
			}

			UClass* AssetClass = MCPReflection::ResolveClass(AssetClassName);
			if (!AssetClass)
			{
				Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
				MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidClassReference,
					FString::Printf(TEXT("Could not resolve asset class: %s"), *AssetClassName));
				return Envelope;
			}

			UPackage* Package = CreatePackage(*FullPath);
			if (!Package)
			{
				Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
				MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
					TEXT("Failed to create package"));
				return Envelope;
			}

			FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Create Data Asset")));

			UObject* NewObj = NewObject<UObject>(Package, AssetClass, FName(*AssetName), RF_Public | RF_Standalone);
			if (NewObj)
			{
				FAssetRegistryModule::AssetCreated(NewObj);
				NewObj->MarkPackageDirty();
				CreatedAsset = NewObj;
			}
		}
		else if (AssetType == TEXT("MaterialInstance"))
		{
			FString ParentMaterialPath;
			if (InitObj.IsValid() && InitObj->HasField(TEXT("parent_material")))
			{
				ParentMaterialPath = InitObj->GetStringField(TEXT("parent_material"));
			}

			UMaterialInterface* ParentMaterial = nullptr;
			if (!ParentMaterialPath.IsEmpty())
			{
				FString NormalizedPath = MCPAssetPathValidator::NormalizeObjectPath(ParentMaterialPath);
				ParentMaterial = LoadObject<UMaterialInterface>(nullptr, *NormalizedPath);
			}

			if (!ParentMaterial)
			{
				// Try loading the default material as a fallback if no parent specified
				if (ParentMaterialPath.IsEmpty())
				{
					ParentMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
				}

				if (!ParentMaterial)
				{
					Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
					MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidObjectReference,
						FString::Printf(TEXT("Could not load parent material: %s"), *ParentMaterialPath));
					return Envelope;
				}
			}

			UPackage* Package = CreatePackage(*FullPath);
			if (!Package)
			{
				Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
				MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
					TEXT("Failed to create package"));
				return Envelope;
			}

			FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Create Material Instance")));

			UMaterialInstanceConstant* NewMIC = NewObject<UMaterialInstanceConstant>(
				Package, FName(*AssetName), RF_Public | RF_Standalone);
			if (NewMIC)
			{
				NewMIC->SetParentEditorOnly(ParentMaterial);
				FAssetRegistryModule::AssetCreated(NewMIC);
				NewMIC->MarkPackageDirty();
				CreatedAsset = NewMIC;
			}
		}

		if (!CreatedAsset)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				FString::Printf(TEXT("Failed to create asset of type %s"), *AssetType));
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false, {}, {}, false, true);
		Envelope->SetStringField(TEXT("asset_type"), AssetType);
		Envelope->SetStringField(TEXT("asset_path"), CreatedAsset->GetPathName());
		Envelope->SetStringField(TEXT("asset_class"), CreatedAsset->GetClass()->GetPathName());
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_create_asset requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_duplicate_asset
// ============================================================================
namespace DuplicateAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_duplicate_asset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Duplicate an existing asset to a new location.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_duplicate_asset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString SourceAsset = MCPToolExecution::GetStringParam(Args, TEXT("source_asset"));
		FString TargetPackagePath = MCPToolExecution::GetStringParam(Args, TEXT("target_package_path"));
		FString TargetAssetName = MCPToolExecution::GetStringParam(Args, TEXT("target_asset_name"));

		// Validate source path
		FString NormalizedSource = MCPAssetPathValidator::NormalizeObjectPath(SourceAsset);

		// Validate target
		FString PathError = MCPAssetPathValidator::ValidatePackagePath(TargetPackagePath);
		if (!PathError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath, PathError);
			return Envelope;
		}

		FString NameError = MCPAssetPathValidator::ValidateAssetName(TargetAssetName);
		if (!NameError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidAssetName, NameError);
			return Envelope;
		}

		// Check source exists
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FAssetData SourceData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(NormalizedSource));
		if (!SourceData.IsValid())
		{
			// Try as package name
			TArray<FAssetData> ByPackage;
			AssetRegistry.GetAssetsByPackageName(FName(*SourceAsset), ByPackage);
			if (ByPackage.Num() > 0)
			{
				SourceData = ByPackage[0];
			}
		}

		if (!SourceData.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Source asset not found: %s"), *SourceAsset));
			return Envelope;
		}

		// Check target does not exist
		if (MCPAssetPathValidator::DoesAssetExist(TargetPackagePath, TargetAssetName))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::NameConflict,
				FString::Printf(TEXT("Target asset already exists: %s/%s"), *TargetPackagePath, *TargetAssetName));
			return Envelope;
		}

#if WITH_EDITOR
		UObject* SourceObject = SourceData.GetAsset();
		if (!SourceObject)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Failed to load source asset: %s"), *SourceAsset));
			return Envelope;
		}

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Duplicate Asset")));

		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		UObject* DuplicatedAsset = AssetTools.DuplicateAsset(TargetAssetName, TargetPackagePath, SourceObject);

		if (!DuplicatedAsset)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("DuplicateAsset returned null"));
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false, {}, {}, false, true);
		Envelope->SetStringField(TEXT("source_asset"), SourceData.GetObjectPathString());
		Envelope->SetStringField(TEXT("new_asset_path"), DuplicatedAsset->GetPathName());
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_duplicate_asset requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_rename_asset
// ============================================================================
namespace RenameAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_rename_asset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Rename an existing asset. Updates all references.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_rename_asset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		FString NewName = MCPToolExecution::GetStringParam(Args, TEXT("new_name"));

		// Validate new name
		FString NameError = MCPAssetPathValidator::ValidateAssetName(NewName);
		if (!NameError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidAssetName, NameError);
			return Envelope;
		}

		// Load source
		FString NormalizedPath = MCPAssetPathValidator::NormalizeObjectPath(AssetPath);
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FAssetData SourceData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(NormalizedPath));
		if (!SourceData.IsValid())
		{
			TArray<FAssetData> ByPackage;
			AssetRegistry.GetAssetsByPackageName(FName(*AssetPath), ByPackage);
			if (ByPackage.Num() > 0) SourceData = ByPackage[0];
		}

		if (!SourceData.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
			return Envelope;
		}

		// Check for name conflict in same directory
		FString PackagePath = FPackageName::GetLongPackagePath(SourceData.PackageName.ToString());
		if (MCPAssetPathValidator::DoesAssetExist(PackagePath, NewName))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::NameConflict,
				FString::Printf(TEXT("An asset named '%s' already exists in %s"), *NewName, *PackagePath));
			return Envelope;
		}

#if WITH_EDITOR
		UObject* SourceObject = SourceData.GetAsset();
		if (!SourceObject)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
			return Envelope;
		}

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Rename Asset")));

		TArray<FAssetRenameData> RenameData;
		FString NewPackagePath = PackagePath / NewName;
		RenameData.Emplace(SourceObject, PackagePath, NewName);

		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		bool bSuccess = AssetTools.RenameAssets(RenameData);

		if (!bSuccess)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("RenameAssets failed"));
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false, {}, {}, false, true);
		Envelope->SetStringField(TEXT("old_path"), SourceData.GetObjectPathString());
		Envelope->SetStringField(TEXT("new_name"), NewName);
		Envelope->SetStringField(TEXT("new_package_path"), PackagePath / NewName);
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_rename_asset requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_move_asset
// ============================================================================
namespace MoveAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_move_asset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Move an asset to a different content folder. Updates all references.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_move_asset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		FString TargetPackagePath = MCPToolExecution::GetStringParam(Args, TEXT("target_package_path"));

		// Validate target
		FString PathError = MCPAssetPathValidator::ValidatePackagePath(TargetPackagePath);
		if (!PathError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath, PathError);
			return Envelope;
		}

		// Load source
		FString NormalizedPath = MCPAssetPathValidator::NormalizeObjectPath(AssetPath);
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FAssetData SourceData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(NormalizedPath));
		if (!SourceData.IsValid())
		{
			TArray<FAssetData> ByPackage;
			AssetRegistry.GetAssetsByPackageName(FName(*AssetPath), ByPackage);
			if (ByPackage.Num() > 0) SourceData = ByPackage[0];
		}

		if (!SourceData.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
			return Envelope;
		}

		FString AssetName = SourceData.AssetName.ToString();

		// Check for conflict at target
		if (MCPAssetPathValidator::DoesAssetExist(TargetPackagePath, AssetName))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::NameConflict,
				FString::Printf(TEXT("Asset '%s' already exists in %s"), *AssetName, *TargetPackagePath));
			return Envelope;
		}

#if WITH_EDITOR
		UObject* SourceObject = SourceData.GetAsset();
		if (!SourceObject)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
			return Envelope;
		}

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Move Asset")));

		TArray<FAssetRenameData> RenameData;
		RenameData.Emplace(SourceObject, TargetPackagePath, AssetName);

		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		bool bSuccess = AssetTools.RenameAssets(RenameData);

		if (!bSuccess)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("MoveAsset (RenameAssets) failed"));
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false, {}, {}, false, true);
		Envelope->SetStringField(TEXT("old_path"), SourceData.GetObjectPathString());
		Envelope->SetStringField(TEXT("new_package_path"), TargetPackagePath / AssetName);
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_move_asset requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_save_asset
// ============================================================================
namespace SaveAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_save_asset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Save one or more assets to disk by their package paths.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_save_asset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		TArray<FString> AssetPaths = MCPToolExecution::GetStringArrayParam(Args, TEXT("asset_paths"));

		if (AssetPaths.Num() == 0)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing or empty required parameter: asset_paths"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> SavedArr;
		TArray<TSharedPtr<FJsonValue>> FailedArr;

		for (const FString& Path : AssetPaths)
		{
			FString PathError = MCPAssetPathValidator::ValidatePackagePath(Path);
			if (!PathError.IsEmpty())
			{
				TSharedPtr<FJsonObject> FailItem = MakeShared<FJsonObject>();
				FailItem->SetStringField(TEXT("path"), Path);
				FailItem->SetStringField(TEXT("error"), PathError);
				FailedArr.Add(MakeShared<FJsonValueObject>(FailItem));
				continue;
			}

			UPackage* Package = FindPackage(nullptr, *Path);
			if (!Package)
			{
				TSharedPtr<FJsonObject> FailItem = MakeShared<FJsonObject>();
				FailItem->SetStringField(TEXT("path"), Path);
				FailItem->SetStringField(TEXT("error"), TEXT("Package not found or not loaded"));
				FailedArr.Add(MakeShared<FJsonValueObject>(FailItem));
				continue;
			}

			FString PackageFilename;
			if (!FPackageName::TryConvertLongPackageNameToFilename(Path, PackageFilename, FPackageName::GetAssetPackageExtension()))
			{
				TSharedPtr<FJsonObject> FailItem = MakeShared<FJsonObject>();
				FailItem->SetStringField(TEXT("path"), Path);
				FailItem->SetStringField(TEXT("error"), TEXT("Could not resolve filename for package"));
				FailedArr.Add(MakeShared<FJsonValueObject>(FailItem));
				continue;
			}

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			bool bSaved = UPackage::SavePackage(Package, nullptr, *PackageFilename, SaveArgs);

			if (bSaved)
			{
				SavedArr.Add(MakeShared<FJsonValueString>(Path));
			}
			else
			{
				TSharedPtr<FJsonObject> FailItem = MakeShared<FJsonObject>();
				FailItem->SetStringField(TEXT("path"), Path);
				FailItem->SetStringField(TEXT("error"), TEXT("SavePackage returned false"));
				FailedArr.Add(MakeShared<FJsonValueObject>(FailItem));
			}
		}

		bool bAllOk = (FailedArr.Num() == 0);
		Envelope = MCPEditEnvelope::MakeEditEnvelope(bAllOk, false);
		Envelope->SetArrayField(TEXT("saved"), SavedArr);
		Envelope->SetArrayField(TEXT("failed"), FailedArr);
		return Envelope;
	}
}

// ============================================================================
// ue_delete_asset
// ============================================================================
namespace DeleteAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_delete_asset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Delete an asset. Optionally preview dependencies or force-delete despite references.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_delete_asset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		bool bPreviewDeps = MCPToolExecution::GetBoolParam(Args, TEXT("preview_dependencies"), false);
		bool bForceDelete = MCPToolExecution::GetBoolParam(Args, TEXT("force_delete"), false);

		// Load asset
		FString NormalizedPath = MCPAssetPathValidator::NormalizeObjectPath(AssetPath);
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FAssetData SourceData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(NormalizedPath));
		if (!SourceData.IsValid())
		{
			TArray<FAssetData> ByPackage;
			AssetRegistry.GetAssetsByPackageName(FName(*AssetPath), ByPackage);
			if (ByPackage.Num() > 0) SourceData = ByPackage[0];
		}

		if (!SourceData.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
			return Envelope;
		}

		// Get referencers
		FName PackageName = SourceData.PackageName;
		TArray<FName> ReferencerNames;
		AssetRegistry.GetReferencers(PackageName, ReferencerNames,
			UE::AssetRegistry::EDependencyCategory::Package);

		// Remove self-references
		ReferencerNames.Remove(PackageName);

		// Build referencers list
		TArray<TSharedPtr<FJsonValue>> ReferencersList;
		for (const FName& RefPkg : ReferencerNames)
		{
			ReferencersList.Add(MakeShared<FJsonValueString>(RefPkg.ToString()));
		}

		// Preview mode: just return dependency info
		if (bPreviewDeps)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(true, true);
			Envelope->SetStringField(TEXT("asset_path"), SourceData.GetObjectPathString());
			Envelope->SetArrayField(TEXT("referencers"), ReferencersList);
			Envelope->SetNumberField(TEXT("referencer_count"), ReferencerNames.Num());
			Envelope->SetBoolField(TEXT("safe_to_delete"), ReferencerNames.Num() == 0);
			return Envelope;
		}

		// Block if has references and not force
		if (ReferencerNames.Num() > 0 && !bForceDelete)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::DeleteBlockedByReferences,
				FString::Printf(TEXT("Asset has %d referencer(s). Use force_delete=true or preview_dependencies=true to see them."),
					ReferencerNames.Num()));
			Envelope->SetArrayField(TEXT("referencers"), ReferencersList);
			return Envelope;
		}

#if WITH_EDITOR
		UObject* AssetObject = SourceData.GetAsset();
		if (!AssetObject)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::AssetNotFound,
				FString::Printf(TEXT("Failed to load asset for deletion: %s"), *AssetPath));
			return Envelope;
		}

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Delete Asset")));

		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(AssetObject);
		int32 DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);

		if (DeletedCount == 0)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("DeleteObjects returned 0 — deletion may have failed"));
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);
		Envelope->SetStringField(TEXT("deleted_asset"), AssetPath);
		Envelope->SetNumberField(TEXT("deleted_count"), DeletedCount);
		Envelope->SetBoolField(TEXT("force_delete"), bForceDelete);
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_delete_asset requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_fix_redirectors
// ============================================================================
namespace FixRedirectors
{
	static constexpr const TCHAR* ToolName = TEXT("ue_fix_redirectors");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Fix up redirectors in a folder, updating all references to point directly to the target assets.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_fix_redirectors");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString FolderPath = MCPToolExecution::GetStringParam(Args, TEXT("folder_path"));

		FString PathError = MCPAssetPathValidator::ValidatePackagePath(FolderPath);
		if (!PathError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath, PathError);
			return Envelope;
		}

#if WITH_EDITOR
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		// Find all redirectors in the folder
		FARFilter Filter;
		Filter.PackagePaths.Add(FName(*FolderPath));
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("ObjectRedirector")));

		TArray<FAssetData> RedirectorAssets;
		AssetRegistry.GetAssets(Filter, RedirectorAssets);

		if (RedirectorAssets.Num() == 0)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);
			Envelope->SetNumberField(TEXT("redirectors_found"), 0);
			Envelope->SetStringField(TEXT("message"), TEXT("No redirectors found in the specified folder."));
			return Envelope;
		}

		// Load the redirector objects
		TArray<UObjectRedirector*> Redirectors;
		for (const FAssetData& AD : RedirectorAssets)
		{
			UObject* Obj = AD.GetAsset();
			if (UObjectRedirector* Redir = Cast<UObjectRedirector>(Obj))
			{
				Redirectors.Add(Redir);
			}
		}

		// Fix up references
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		AssetTools.FixupReferencers(Redirectors);

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);
		Envelope->SetNumberField(TEXT("redirectors_found"), RedirectorAssets.Num());
		Envelope->SetNumberField(TEXT("redirectors_processed"), Redirectors.Num());
		Envelope->SetStringField(TEXT("folder_path"), FolderPath);
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_fix_redirectors requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_get_asset_create_schema
// ============================================================================
namespace GetAssetCreateSchema
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_asset_create_schema");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get the creation schema for supported asset types. Call with no arguments to list all types.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_asset_create_schema");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetType = MCPToolExecution::GetStringParam(Args, TEXT("asset_type"));

		TSharedPtr<FJsonObject> Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);

		if (AssetType.IsEmpty())
		{
			// Return all supported types
			TArray<TSharedPtr<FJsonValue>> Types;

			auto AddType = [&Types](const FString& Name, const FString& Desc)
			{
				TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
				TypeObj->SetStringField(TEXT("type"), Name);
				TypeObj->SetStringField(TEXT("description"), Desc);
				Types.Add(MakeShared<FJsonValueObject>(TypeObj));
			};

			AddType(TEXT("BlueprintClass"), TEXT("A Blueprint class asset. Optionally specify init.parent_class (default: Actor)."));
			AddType(TEXT("DataAsset"), TEXT("A DataAsset or subclass. Optionally specify init.asset_class (default: UDataAsset)."));
			AddType(TEXT("MaterialInstance"), TEXT("A Material Instance Constant. Optionally specify init.parent_material (path to parent material)."));

			Envelope->SetArrayField(TEXT("supported_types"), Types);
			return Envelope;
		}

		// Return schema for a specific type
		TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("asset_type"), AssetType);

		if (AssetType == TEXT("BlueprintClass"))
		{
			Schema->SetStringField(TEXT("description"), TEXT("Creates a new Blueprint class."));

			TSharedPtr<FJsonObject> InitSchema = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> ParentProp = MakeShared<FJsonObject>();
			ParentProp->SetStringField(TEXT("type"), TEXT("string"));
			ParentProp->SetStringField(TEXT("description"), TEXT("Parent class name or path. Default: Actor"));
			ParentProp->SetStringField(TEXT("default"), TEXT("Actor"));
			InitSchema->SetObjectField(TEXT("parent_class"), ParentProp);
			Schema->SetObjectField(TEXT("init_properties"), InitSchema);
		}
		else if (AssetType == TEXT("DataAsset"))
		{
			Schema->SetStringField(TEXT("description"), TEXT("Creates a new DataAsset instance."));

			TSharedPtr<FJsonObject> InitSchema = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> ClassProp = MakeShared<FJsonObject>();
			ClassProp->SetStringField(TEXT("type"), TEXT("string"));
			ClassProp->SetStringField(TEXT("description"), TEXT("UDataAsset subclass name or path. Default: UDataAsset"));
			ClassProp->SetStringField(TEXT("default"), TEXT("UDataAsset"));
			InitSchema->SetObjectField(TEXT("asset_class"), ClassProp);
			Schema->SetObjectField(TEXT("init_properties"), InitSchema);
		}
		else if (AssetType == TEXT("MaterialInstance"))
		{
			Schema->SetStringField(TEXT("description"), TEXT("Creates a new Material Instance Constant."));

			TSharedPtr<FJsonObject> InitSchema = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> ParentProp = MakeShared<FJsonObject>();
			ParentProp->SetStringField(TEXT("type"), TEXT("string"));
			ParentProp->SetStringField(TEXT("description"), TEXT("Package path to parent material (e.g. /Game/Materials/M_Base). If omitted, uses the default engine material."));
			InitSchema->SetObjectField(TEXT("parent_material"), ParentProp);
			Schema->SetObjectField(TEXT("init_properties"), InitSchema);
		}
		else
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::UnsupportedAssetType,
				FString::Printf(TEXT("Unknown asset_type: %s"), *AssetType));
			return Envelope;
		}

		Envelope->SetObjectField(TEXT("schema"), Schema);
		return Envelope;
	}
}

// ============================================================================
// ue_undo
// ============================================================================
namespace Undo
{
	static constexpr const TCHAR* ToolName = TEXT("ue_undo");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Undo the last editor transaction(s). Specify count to undo multiple steps.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_undo");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		int32 Count = MCPToolExecution::GetIntParam(Args, TEXT("count"), 1);
		if (Count < 1)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("count must be >= 1"));
			return Envelope;
		}

#if WITH_EDITOR
		int32 Undone = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			if (GEditor && GEditor->UndoTransaction())
			{
				++Undone;
			}
			else
			{
				break;
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Undone > 0, false);
		Envelope->SetNumberField(TEXT("undone"), Undone);
		Envelope->SetNumberField(TEXT("requested"), Count);
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_undo requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_redo
// ============================================================================
namespace Redo
{
	static constexpr const TCHAR* ToolName = TEXT("ue_redo");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Redo previously undone editor transaction(s). Specify count to redo multiple steps.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_redo");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		int32 Count = MCPToolExecution::GetIntParam(Args, TEXT("count"), 1);
		if (Count < 1)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("count must be >= 1"));
			return Envelope;
		}

#if WITH_EDITOR
		int32 Redone = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			if (GEditor && GEditor->RedoTransaction())
			{
				++Redone;
			}
			else
			{
				break;
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Redone > 0, false);
		Envelope->SetNumberField(TEXT("redone"), Redone);
		Envelope->SetNumberField(TEXT("requested"), Count);
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_redo requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_AssetEdit::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
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

	RegisterGameThreadTool(CreateFolder::ToolName, CreateFolder::ToolDescription, CreateFolder::SchemaDefName, CreateFolder::Execute);
	RegisterGameThreadTool(CreateAsset::ToolName, CreateAsset::ToolDescription, CreateAsset::SchemaDefName, CreateAsset::Execute);
	RegisterGameThreadTool(DuplicateAsset::ToolName, DuplicateAsset::ToolDescription, DuplicateAsset::SchemaDefName, DuplicateAsset::Execute);
	RegisterGameThreadTool(RenameAsset::ToolName, RenameAsset::ToolDescription, RenameAsset::SchemaDefName, RenameAsset::Execute);
	RegisterGameThreadTool(MoveAsset::ToolName, MoveAsset::ToolDescription, MoveAsset::SchemaDefName, MoveAsset::Execute);
	RegisterGameThreadTool(SaveAsset::ToolName, SaveAsset::ToolDescription, SaveAsset::SchemaDefName, SaveAsset::Execute);
	RegisterGameThreadTool(DeleteAsset::ToolName, DeleteAsset::ToolDescription, DeleteAsset::SchemaDefName, DeleteAsset::Execute);
	RegisterGameThreadTool(FixRedirectors::ToolName, FixRedirectors::ToolDescription, FixRedirectors::SchemaDefName, FixRedirectors::Execute);
	RegisterGameThreadTool(GetAssetCreateSchema::ToolName, GetAssetCreateSchema::ToolDescription, GetAssetCreateSchema::SchemaDefName, GetAssetCreateSchema::Execute);
	RegisterGameThreadTool(Undo::ToolName, Undo::ToolDescription, Undo::SchemaDefName, Undo::Execute);
	RegisterGameThreadTool(Redo::ToolName, Redo::ToolDescription, Redo::SchemaDefName, Redo::Execute);
}
