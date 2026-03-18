// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_DataAsset.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Services/MCPStructJsonConverter.h"
#include "MCPToolRegistry.h"
#include "Edit/MCPAssetPathValidator.h"
#include "Edit/MCPEditErrors.h"
#include "Edit/MCPEditEnvelope.h"
#include "Reflection/MCPReflectionCore.h"

#include "Engine/DataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMCPDataAsset, Log, All);

// ============================================================================
// Local helpers
// ============================================================================

namespace MCPToolDataAssetPrivate
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

	UDataAsset* LoadDataAsset(const FString& AssetPath, TSharedPtr<FJsonObject>& OutError)
	{
		FString NormalizedPath = MCPAssetPathValidator::NormalizeObjectPath(AssetPath);
		UObject* Loaded = FSoftObjectPath(NormalizedPath).TryLoad();
		UDataAsset* DA = Cast<UDataAsset>(Loaded);
		if (!DA)
		{
			OutError = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(OutError,
				Loaded ? MCPEditErrors::InvalidAssetType : MCPEditErrors::AssetNotFound,
				Loaded ? FString::Printf(TEXT("Asset is not a DataAsset: %s"), *AssetPath)
				       : FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		}
		return DA;
	}

	bool SaveAssetPackage(UObject* Asset, FString& OutError)
	{
		UPackage* Package = Asset->GetOutermost();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
		if (!bSaved) OutError = TEXT("SavePackage failed");
		return bSaved;
	}

	bool IsUserProperty(const FProperty* Property)
	{
		UStruct* Owner = Property->GetOwnerStruct();
		if (Owner == UObject::StaticClass() || Owner == UDataAsset::StaticClass())
			return false;
		if (!(Property->PropertyFlags & (CPF_Edit | CPF_BlueprintVisible)))
			return false;
		return true;
	}

	struct FCollectionInfo
	{
		FArrayProperty* ArrayProp = nullptr;
		FStructProperty* InnerStructProp = nullptr;
		FProperty* KeyProperty = nullptr;
		FString CollectionFieldName;
		FString KeyFieldName;
		bool IsValid() const { return ArrayProp && InnerStructProp && KeyProperty; }
	};

	FCollectionInfo FindCollectionProperty(UDataAsset* DataAsset)
	{
		FCollectionInfo Info;
		UClass* Class = DataAsset->GetClass();
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			if (!IsUserProperty(*It)) continue;
			FArrayProperty* ArrProp = CastField<FArrayProperty>(*It);
			if (!ArrProp) continue;
			FStructProperty* InnerProp = CastField<FStructProperty>(ArrProp->Inner);
			if (!InnerProp) continue;

			Info.ArrayProp = ArrProp;
			Info.InnerStructProp = InnerProp;
			Info.CollectionFieldName = ArrProp->GetName();

			for (TFieldIterator<FProperty> FieldIt(InnerProp->Struct); FieldIt; ++FieldIt)
			{
				if (CastField<FNameProperty>(*FieldIt) || (*FieldIt)->GetClass()->GetFName() == TEXT("StrProperty"))
				{
					Info.KeyProperty = *FieldIt;
					Info.KeyFieldName = (*FieldIt)->GetName();
					break;
				}
			}
			break;
		}
		return Info;
	}

	FString ReadKeyValue(const FProperty* KeyProp, const void* StructPtr)
	{
		const void* ValPtr = KeyProp->ContainerPtrToValuePtr<void>(StructPtr);
		if (const FNameProperty* NameProp = CastField<FNameProperty>(KeyProp))
			return NameProp->GetPropertyValue(ValPtr).ToString();
		if (KeyProp->GetClass()->GetFName() == TEXT("StrProperty"))
			return *static_cast<const FString*>(ValPtr);
		return TEXT("");
	}

	void WriteKeyValue(const FProperty* KeyProp, void* StructPtr, const FString& Value)
	{
		void* ValPtr = const_cast<FProperty*>(KeyProp)->ContainerPtrToValuePtr<void>(StructPtr);
		if (FNameProperty* NameProp = CastField<FNameProperty>(const_cast<FProperty*>(KeyProp)))
			NameProp->SetPropertyValue(ValPtr, FName(*Value));
		else if (KeyProp->GetClass()->GetFName() == TEXT("StrProperty"))
			*static_cast<FString*>(ValPtr) = Value;
	}
}
using namespace MCPToolDataAssetPrivate;

// ============================================================================
// ue_get_dataasset_schema
// ============================================================================
namespace GetDataAssetSchema
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_dataasset_schema");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get the editable field schema of a DataAsset, including field names, types, and editability.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_dataasset_schema");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		UClass* Class = DataAsset->GetClass();
		TArray<TSharedPtr<FJsonValue>> Fields = MCPStructJsonConverter::BuildFieldSchemaArrayForClass(Class);

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetStringField(TEXT("dataasset_asset"), AssetPath);
		Envelope->SetStringField(TEXT("asset_class"), Class->GetPathName());
		Envelope->SetStringField(TEXT("mode"), TEXT("record"));
		Envelope->SetArrayField(TEXT("fields"), Fields);
		return Envelope;
	}
}

// ============================================================================
// ue_get_dataasset_payload
// ============================================================================
namespace GetDataAssetPayload
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_dataasset_payload");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Read all editable field values from a DataAsset as a JSON payload.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_dataasset_payload");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(DataAsset->GetClass()); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!IsUserProperty(Prop)) continue;

			const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(DataAsset);
			TSharedPtr<FJsonValue> JsonVal = MCPStructJsonConverter::PropertyToJsonValue(Prop, ValuePtr);
			Payload->SetField(Prop->GetName(), JsonVal);
		}

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetStringField(TEXT("dataasset_asset"), AssetPath);
		Envelope->SetObjectField(TEXT("payload"), Payload);
		return Envelope;
	}
}

// ============================================================================
// ue_create_dataasset
// ============================================================================
namespace CreateDataAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_create_dataasset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Create a new DataAsset of a specified class.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_create_dataasset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString PackagePath = MCPToolExecution::GetStringParam(Args, TEXT("package_path"));
		FString AssetName = MCPToolExecution::GetStringParam(Args, TEXT("asset_name"));
		FString AssetClassName = MCPToolExecution::GetStringParam(Args, TEXT("asset_class"));

		FString PathError = MCPAssetPathValidator::ValidatePackagePath(PackagePath);
		if (!PathError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidPackagePath, PathError);
			return Envelope;
		}

		FString NameError = MCPAssetPathValidator::ValidateAssetName(AssetName);
		if (!NameError.IsEmpty())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::InvalidAssetName, NameError);
			return Envelope;
		}

		if (MCPAssetPathValidator::DoesAssetExist(PackagePath, AssetName))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::NameConflict,
				FString::Printf(TEXT("Asset already exists: %s/%s"), *PackagePath, *AssetName));
			return Envelope;
		}

		UClass* AssetClass = MCPReflection::ResolveClass(AssetClassName);
		if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass()))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::UnsupportedDataAssetClass,
				FString::Printf(TEXT("Could not resolve DataAsset class: %s"), *AssetClassName));
			return Envelope;
		}

#if WITH_EDITOR
		FString FullPath = PackagePath / AssetName;

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Create DataAsset")));

		UPackage* Package = CreatePackage(*FullPath);
		if (!Package)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("Failed to create package"));
			return Envelope;
		}

		UObject* NewObj = NewObject<UObject>(Package, AssetClass, FName(*AssetName), RF_Public | RF_Standalone);
		if (!NewObj)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("Failed to create DataAsset object"));
			return Envelope;
		}

		FAssetRegistryModule::AssetCreated(NewObj);
		NewObj->MarkPackageDirty();

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false, {}, {}, false, true);
		Envelope->SetStringField(TEXT("dataasset_asset"), FullPath + TEXT(".") + AssetName);
		Envelope->SetStringField(TEXT("asset_class"), AssetClass->GetPathName());
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_create_dataasset requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_patch_dataasset_fields
// ============================================================================
namespace PatchDataAssetFields
{
	static constexpr const TCHAR* ToolName = TEXT("ue_patch_dataasset_fields");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Patch specific fields on a DataAsset by property path and value. Supports dry_run.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_patch_dataasset_fields");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		// Extract patch array
		TArray<TSharedPtr<FJsonValue>> PatchArray;
		if (Args.IsValid() && Args->HasField(TEXT("patch")))
		{
			PatchArray = Args->GetArrayField(TEXT("patch"));
		}

		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Warnings;
		TArray<FString> Errors;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Patch DataAsset Fields")), !bDryRun);
#endif

		for (const TSharedPtr<FJsonValue>& PatchVal : PatchArray)
		{
			const TSharedPtr<FJsonObject>& PatchObj = PatchVal->AsObject();
			if (!PatchObj.IsValid()) continue;

			FString PropertyPath = PatchObj->GetStringField(TEXT("property_path"));
			TSharedPtr<FJsonValue> NewValue = PatchObj->TryGetField(TEXT("value"));

			if (PropertyPath.IsEmpty() || !NewValue.IsValid())
			{
				Errors.Add(FString::Printf(TEXT("Invalid patch entry: missing property_path or value")));
				continue;
			}

			// Find property on class
			FProperty* Prop = DataAsset->GetClass()->FindPropertyByName(*PropertyPath);
			if (!Prop)
			{
				Errors.Add(FString::Printf(TEXT("Property '%s' not found"), *PropertyPath));
				continue;
			}

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(DataAsset);

			// Read old value
			TSharedPtr<FJsonValue> OldValue = MCPStructJsonConverter::PropertyToJsonValue(Prop, ValuePtr);

			if (!bDryRun)
			{
				DataAsset->PreEditChange(Prop);

				TArray<FString> WriteErrors;
				MCPStructJsonConverter::JsonValueToProperty(NewValue, Prop, ValuePtr, WriteErrors);

				FPropertyChangedEvent ChangedEvent(Prop);
				DataAsset->PostEditChangeProperty(ChangedEvent);

				for (const FString& E : WriteErrors)
				{
					Errors.Add(FString::Printf(TEXT("Property '%s': %s"), *PropertyPath, *E));
				}
			}

			TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
			Change->SetStringField(TEXT("target"), AssetPath);
			Change->SetStringField(TEXT("op"), TEXT("update_field"));
			Change->SetStringField(TEXT("field"), PropertyPath);
			Change->SetField(TEXT("old"), OldValue);
			Change->SetField(TEXT("new"), NewValue);
			Changes.Add(MakeShared<FJsonValueObject>(Change));
		}

		if (!bDryRun && Changes.Num() > 0)
		{
			DataAsset->MarkPackageDirty();

			if (bSave)
			{
				FString SaveError;
				if (!SaveAssetPackage(DataAsset, SaveError))
				{
					Warnings.Add(FString::Printf(TEXT("Auto-save failed: %s"), *SaveError));
				}
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Errors.Num() == 0, bDryRun, Changes, Warnings, false, !bDryRun && Changes.Num() > 0 && !bSave);

		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorValues;
			for (const FString& E : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(E));
			Envelope->SetArrayField(TEXT("errors"), ErrorValues);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_replace_dataasset_payload
// ============================================================================
namespace ReplaceDataAssetPayload
{
	static constexpr const TCHAR* ToolName = TEXT("ue_replace_dataasset_payload");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Replace all editable fields on a DataAsset with the provided payload. Supports dry_run.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_replace_dataasset_payload");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
		if (!Args.IsValid() || !Args->TryGetObjectField(TEXT("payload"), PayloadPtr) || !PayloadPtr)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: payload"));
			return Envelope;
		}

		const TSharedPtr<FJsonObject>& Payload = *PayloadPtr;

		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Warnings;
		TArray<FString> Errors;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Replace DataAsset Payload")), !bDryRun);
#endif

		// Process each key in payload
		for (const auto& Pair : Payload->Values)
		{
			FProperty* Prop = DataAsset->GetClass()->FindPropertyByName(*Pair.Key);
			if (!Prop)
			{
				Errors.Add(FString::Printf(TEXT("Property '%s' not found"), *Pair.Key));
				continue;
			}

			if (!IsUserProperty(Prop))
			{
				Warnings.Add(FString::Printf(TEXT("Property '%s' is not editable, skipped"), *Pair.Key));
				continue;
			}

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(DataAsset);
			TSharedPtr<FJsonValue> OldValue = MCPStructJsonConverter::PropertyToJsonValue(Prop, ValuePtr);

			if (!bDryRun)
			{
				DataAsset->PreEditChange(Prop);

				TArray<FString> WriteErrors;
				MCPStructJsonConverter::JsonValueToProperty(Pair.Value, Prop, ValuePtr, WriteErrors);

				FPropertyChangedEvent ChangedEvent(Prop);
				DataAsset->PostEditChangeProperty(ChangedEvent);

				for (const FString& E : WriteErrors)
				{
					Errors.Add(FString::Printf(TEXT("Property '%s': %s"), *Pair.Key, *E));
				}
			}

			TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
			Change->SetStringField(TEXT("target"), AssetPath);
			Change->SetStringField(TEXT("op"), TEXT("update_field"));
			Change->SetStringField(TEXT("field"), Pair.Key);
			Change->SetField(TEXT("old"), OldValue);
			Change->SetField(TEXT("new"), Pair.Value);
			Changes.Add(MakeShared<FJsonValueObject>(Change));
		}

		// Warn about editable properties not in payload
		for (TFieldIterator<FProperty> It(DataAsset->GetClass()); It; ++It)
		{
			if (!IsUserProperty(*It)) continue;
			if (!Payload->HasField((*It)->GetName()))
			{
				Warnings.Add(FString::Printf(TEXT("Editable property '%s' not included in payload"), *(*It)->GetName()));
			}
		}

		if (!bDryRun && Changes.Num() > 0)
		{
			DataAsset->MarkPackageDirty();

			if (bSave)
			{
				FString SaveError;
				if (!SaveAssetPackage(DataAsset, SaveError))
				{
					Warnings.Add(FString::Printf(TEXT("Auto-save failed: %s"), *SaveError));
				}
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Errors.Num() == 0, bDryRun, Changes, Warnings, false, !bDryRun && Changes.Num() > 0 && !bSave);

		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorValues;
			for (const FString& E : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(E));
			Envelope->SetArrayField(TEXT("errors"), ErrorValues);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_save_dataasset
// ============================================================================
namespace SaveDataAsset
{
	static constexpr const TCHAR* ToolName = TEXT("ue_save_dataasset");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Save a DataAsset to disk.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_save_dataasset");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		FString SaveError;
		if (!SaveAssetPackage(DataAsset, SaveError))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SaveFailed, SaveError);
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);
		Envelope->SetStringField(TEXT("dataasset_asset"), AssetPath);
		Envelope->SetStringField(TEXT("message"), TEXT("DataAsset saved successfully"));
		return Envelope;
	}
}

// ============================================================================
// ue_get_dataasset_collection_schema
// ============================================================================
namespace GetDataAssetCollectionSchema
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_dataasset_collection_schema");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get the collection schema of a DataAsset that contains a TArray<Struct> property.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_dataasset_collection_schema");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		FCollectionInfo Info = FindCollectionProperty(DataAsset);
		if (!Info.IsValid())
		{
			TSharedPtr<FJsonObject> Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::CollectionNotFound,
				TEXT("No TArray<Struct> collection property found on this DataAsset"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> Fields = MCPStructJsonConverter::BuildFieldSchemaArray(Info.InnerStructProp->Struct);

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetStringField(TEXT("mode"), TEXT("collection"));
		Envelope->SetStringField(TEXT("collection_field"), Info.CollectionFieldName);
		Envelope->SetStringField(TEXT("key_field"), Info.KeyFieldName);
		Envelope->SetStringField(TEXT("row_struct"), Info.InnerStructProp->Struct->GetPathName());
		Envelope->SetArrayField(TEXT("fields"), Fields);
		return Envelope;
	}
}

// ============================================================================
// ue_get_dataasset_collection_rows
// ============================================================================
namespace GetDataAssetCollectionRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_dataasset_collection_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Read all rows from a collection-like DataAsset's TArray<Struct> property.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_dataasset_collection_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		FCollectionInfo Info = FindCollectionProperty(DataAsset);
		if (!Info.IsValid())
		{
			TSharedPtr<FJsonObject> Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::CollectionNotFound,
				TEXT("No TArray<Struct> collection property found on this DataAsset"));
			return Envelope;
		}

		const void* ArrayValuePtr = Info.ArrayProp->ContainerPtrToValuePtr<void>(DataAsset);
		FScriptArrayHelper ArrayHelper(Info.ArrayProp, ArrayValuePtr);

		TArray<TSharedPtr<FJsonValue>> RowsArray;
		RowsArray.Reserve(ArrayHelper.Num());

		const UScriptStruct* RowStruct = Info.InnerStructProp->Struct;

		for (int32 i = 0; i < ArrayHelper.Num(); ++i)
		{
			const void* ElemPtr = ArrayHelper.GetRawPtr(i);
			FString Key = ReadKeyValue(Info.KeyProperty, ElemPtr);

			TSharedPtr<FJsonObject> DataJson;
			TArray<FString> Errors;
			MCPStructJsonConverter::StructToJson(RowStruct, ElemPtr, DataJson, Errors);

			TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
			RowObj->SetStringField(TEXT("key"), Key);
			RowObj->SetObjectField(TEXT("data"), DataJson);
			RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
		}

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetArrayField(TEXT("rows"), RowsArray);
		return Envelope;
	}
}

// ============================================================================
// ue_upsert_dataasset_collection_rows
// ============================================================================
namespace UpsertDataAssetCollectionRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_upsert_dataasset_collection_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Insert or update rows in a collection-like DataAsset by key. Supports dry_run.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_upsert_dataasset_collection_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		FCollectionInfo Info = FindCollectionProperty(DataAsset);
		if (!Info.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::CollectionNotFound,
				TEXT("No TArray<Struct> collection property found"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> InputRows;
		if (Args.IsValid() && Args->HasField(TEXT("rows")))
		{
			InputRows = Args->GetArrayField(TEXT("rows"));
		}

		void* ArrayValuePtr = Info.ArrayProp->ContainerPtrToValuePtr<void>(DataAsset);
		FScriptArrayHelper ArrayHelper(Info.ArrayProp, ArrayValuePtr);
		const UScriptStruct* RowStruct = Info.InnerStructProp->Struct;

		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Errors;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Upsert DataAsset Collection Rows")), !bDryRun);
#endif

		for (const TSharedPtr<FJsonValue>& RowVal : InputRows)
		{
			const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
			if (!RowObj.IsValid()) continue;

			FString Key = RowObj->GetStringField(TEXT("key"));
			const TSharedPtr<FJsonObject>* DataPtr = nullptr;
			RowObj->TryGetObjectField(TEXT("data"), DataPtr);

			if (!DataPtr || !(*DataPtr).IsValid())
			{
				Errors.Add(FString::Printf(TEXT("Row key '%s': missing data"), *Key));
				continue;
			}

			// Find existing element by key
			int32 FoundIndex = INDEX_NONE;
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				const void* ElemPtr = ArrayHelper.GetRawPtr(i);
				if (ReadKeyValue(Info.KeyProperty, ElemPtr) == Key)
				{
					FoundIndex = i;
					break;
				}
			}

			if (FoundIndex != INDEX_NONE)
			{
				// UPDATE
				if (!bDryRun)
				{
					void* ElemPtr = ArrayHelper.GetRawPtr(FoundIndex);
					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, ElemPtr, WriteErrors, true);
					for (const FString& E : WriteErrors) Errors.Add(FString::Printf(TEXT("Key '%s': %s"), *Key, *E));
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("update_row"));
				Change->SetStringField(TEXT("key"), Key);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
			else
			{
				// INSERT
				if (!bDryRun)
				{
					int32 NewIdx = ArrayHelper.AddValue();
					void* ElemPtr = ArrayHelper.GetRawPtr(NewIdx);

					// Set the key field
					WriteKeyValue(Info.KeyProperty, ElemPtr, Key);

					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, ElemPtr, WriteErrors, true);
					for (const FString& E : WriteErrors) Errors.Add(FString::Printf(TEXT("Key '%s': %s"), *Key, *E));
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("add_row"));
				Change->SetStringField(TEXT("key"), Key);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
		}

		if (!bDryRun && Changes.Num() > 0)
		{
			DataAsset->MarkPackageDirty();
			if (bSave)
			{
				FString SaveError;
				if (!SaveAssetPackage(DataAsset, SaveError))
				{
					UE_LOG(LogMCPDataAsset, Warning, TEXT("Auto-save failed: %s"), *SaveError);
				}
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Errors.Num() == 0, bDryRun, Changes, {}, false, !bDryRun && Changes.Num() > 0 && !bSave);

		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorValues;
			for (const FString& E : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(E));
			Envelope->SetArrayField(TEXT("errors"), ErrorValues);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_replace_dataasset_collection_rows
// ============================================================================
namespace ReplaceDataAssetCollectionRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_replace_dataasset_collection_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Replace all rows in a collection-like DataAsset. Existing rows are cleared and replaced.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_replace_dataasset_collection_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		FCollectionInfo Info = FindCollectionProperty(DataAsset);
		if (!Info.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::CollectionNotFound,
				TEXT("No TArray<Struct> collection property found"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> InputRows;
		if (Args.IsValid() && Args->HasField(TEXT("rows")))
		{
			InputRows = Args->GetArrayField(TEXT("rows"));
		}

		void* ArrayValuePtr = Info.ArrayProp->ContainerPtrToValuePtr<void>(DataAsset);
		FScriptArrayHelper ArrayHelper(Info.ArrayProp, ArrayValuePtr);
		const UScriptStruct* RowStruct = Info.InnerStructProp->Struct;

		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Errors;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Replace DataAsset Collection Rows")), !bDryRun);
#endif

		if (!bDryRun)
		{
			// Record deleted rows
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				const void* ElemPtr = ArrayHelper.GetRawPtr(i);
				FString Key = ReadKeyValue(Info.KeyProperty, ElemPtr);

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("delete_row"));
				Change->SetStringField(TEXT("key"), Key);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}

			// Clear the array
			ArrayHelper.EmptyValues();

			// Add new rows
			for (const TSharedPtr<FJsonValue>& RowVal : InputRows)
			{
				const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
				if (!RowObj.IsValid()) continue;

				FString Key = RowObj->GetStringField(TEXT("key"));
				const TSharedPtr<FJsonObject>* DataPtr = nullptr;
				RowObj->TryGetObjectField(TEXT("data"), DataPtr);

				int32 NewIdx = ArrayHelper.AddValue();
				void* ElemPtr = ArrayHelper.GetRawPtr(NewIdx);

				WriteKeyValue(Info.KeyProperty, ElemPtr, Key);

				if (DataPtr && (*DataPtr).IsValid())
				{
					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, ElemPtr, WriteErrors, true);
					for (const FString& E : WriteErrors) Errors.Add(FString::Printf(TEXT("Key '%s': %s"), *Key, *E));
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("add_row"));
				Change->SetStringField(TEXT("key"), Key);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}

			DataAsset->MarkPackageDirty();
			if (bSave)
			{
				FString SaveError;
				if (!SaveAssetPackage(DataAsset, SaveError))
				{
					UE_LOG(LogMCPDataAsset, Warning, TEXT("Auto-save failed: %s"), *SaveError);
				}
			}
		}
		else
		{
			// Dry run: just describe what would happen
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				const void* ElemPtr = ArrayHelper.GetRawPtr(i);
				FString Key = ReadKeyValue(Info.KeyProperty, ElemPtr);

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("delete_row"));
				Change->SetStringField(TEXT("key"), Key);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
			for (const TSharedPtr<FJsonValue>& RowVal : InputRows)
			{
				const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
				if (!RowObj.IsValid()) continue;

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("add_row"));
				Change->SetStringField(TEXT("key"), RowObj->GetStringField(TEXT("key")));
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Errors.Num() == 0, bDryRun, Changes, {}, false, !bDryRun && !bSave);

		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorValues;
			for (const FString& E : Errors) ErrorValues.Add(MakeShared<FJsonValueString>(E));
			Envelope->SetArrayField(TEXT("errors"), ErrorValues);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_delete_dataasset_collection_rows
// ============================================================================
namespace DeleteDataAssetCollectionRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_delete_dataasset_collection_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Delete rows from a collection-like DataAsset by key. Supports dry_run.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_delete_dataasset_collection_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("dataasset_asset"));
		TArray<FString> Keys = MCPToolExecution::GetStringArrayParam(Args, TEXT("keys"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataAsset* DataAsset = LoadDataAsset(AssetPath, ErrorEnvelope);
		if (!DataAsset) return ErrorEnvelope;

		FCollectionInfo Info = FindCollectionProperty(DataAsset);
		if (!Info.IsValid())
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::CollectionNotFound,
				TEXT("No TArray<Struct> collection property found"));
			return Envelope;
		}

		void* ArrayValuePtr = Info.ArrayProp->ContainerPtrToValuePtr<void>(DataAsset);
		FScriptArrayHelper ArrayHelper(Info.ArrayProp, ArrayValuePtr);

		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Warnings;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Delete DataAsset Collection Rows")), !bDryRun);
#endif

		// Collect indices to delete (from back to front)
		TArray<int32> IndicesToDelete;
		for (const FString& Key : Keys)
		{
			bool bFound = false;
			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				const void* ElemPtr = ArrayHelper.GetRawPtr(i);
				if (ReadKeyValue(Info.KeyProperty, ElemPtr) == Key)
				{
					IndicesToDelete.Add(i);
					bFound = true;

					TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
					Change->SetStringField(TEXT("op"), TEXT("delete_row"));
					Change->SetStringField(TEXT("key"), Key);
					Changes.Add(MakeShared<FJsonValueObject>(Change));
					break;
				}
			}
			if (!bFound)
			{
				Warnings.Add(FString::Printf(TEXT("Key '%s' not found, skipped"), *Key));
			}
		}

		if (!bDryRun && IndicesToDelete.Num() > 0)
		{
			// Sort descending to delete from back to front
			IndicesToDelete.Sort([](int32 A, int32 B) { return A > B; });
			for (int32 Index : IndicesToDelete)
			{
				ArrayHelper.RemoveValues(Index, 1);
			}

			DataAsset->MarkPackageDirty();
			if (bSave)
			{
				FString SaveError;
				if (!SaveAssetPackage(DataAsset, SaveError))
				{
					Warnings.Add(FString::Printf(TEXT("Auto-save failed: %s"), *SaveError));
				}
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, bDryRun, Changes, Warnings, false, !bDryRun && Changes.Num() > 0 && !bSave);
		return Envelope;
	}
}

// ============================================================================
// RegisterAll
// ============================================================================

void MCPTool_DataAsset::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
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

	// Phase 3: Query tools
	RegisterGameThreadTool(GetDataAssetSchema::ToolName, GetDataAssetSchema::ToolDescription, GetDataAssetSchema::SchemaDefName, GetDataAssetSchema::Execute);
	RegisterGameThreadTool(GetDataAssetPayload::ToolName, GetDataAssetPayload::ToolDescription, GetDataAssetPayload::SchemaDefName, GetDataAssetPayload::Execute);

	// Phase 4: Edit tools
	RegisterGameThreadTool(CreateDataAsset::ToolName, CreateDataAsset::ToolDescription, CreateDataAsset::SchemaDefName, CreateDataAsset::Execute);
	RegisterGameThreadTool(PatchDataAssetFields::ToolName, PatchDataAssetFields::ToolDescription, PatchDataAssetFields::SchemaDefName, PatchDataAssetFields::Execute);
	RegisterGameThreadTool(ReplaceDataAssetPayload::ToolName, ReplaceDataAssetPayload::ToolDescription, ReplaceDataAssetPayload::SchemaDefName, ReplaceDataAssetPayload::Execute);
	RegisterGameThreadTool(SaveDataAsset::ToolName, SaveDataAsset::ToolDescription, SaveDataAsset::SchemaDefName, SaveDataAsset::Execute);

	// Phase 5: Collection tools
	RegisterGameThreadTool(GetDataAssetCollectionSchema::ToolName, GetDataAssetCollectionSchema::ToolDescription, GetDataAssetCollectionSchema::SchemaDefName, GetDataAssetCollectionSchema::Execute);
	RegisterGameThreadTool(GetDataAssetCollectionRows::ToolName, GetDataAssetCollectionRows::ToolDescription, GetDataAssetCollectionRows::SchemaDefName, GetDataAssetCollectionRows::Execute);
	RegisterGameThreadTool(UpsertDataAssetCollectionRows::ToolName, UpsertDataAssetCollectionRows::ToolDescription, UpsertDataAssetCollectionRows::SchemaDefName, UpsertDataAssetCollectionRows::Execute);
	RegisterGameThreadTool(ReplaceDataAssetCollectionRows::ToolName, ReplaceDataAssetCollectionRows::ToolDescription, ReplaceDataAssetCollectionRows::SchemaDefName, ReplaceDataAssetCollectionRows::Execute);
	RegisterGameThreadTool(DeleteDataAssetCollectionRows::ToolName, DeleteDataAssetCollectionRows::ToolDescription, DeleteDataAssetCollectionRows::SchemaDefName, DeleteDataAssetCollectionRows::Execute);
}
