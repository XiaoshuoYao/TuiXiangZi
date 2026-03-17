// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_DataTable.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Services/MCPStructJsonConverter.h"
#include "MCPToolRegistry.h"
#include "Edit/MCPAssetPathValidator.h"
#include "Edit/MCPEditErrors.h"
#include "Edit/MCPEditEnvelope.h"

#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMCPDataTable, Log, All);

// ============================================================================
// Local helpers
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

	UDataTable* LoadDataTable(const FString& AssetPath, TSharedPtr<FJsonObject>& OutErrorEnvelope)
	{
		FString NormalizedPath = MCPAssetPathValidator::NormalizeObjectPath(AssetPath);
		UObject* Loaded = FSoftObjectPath(NormalizedPath).TryLoad();
		UDataTable* DataTable = Cast<UDataTable>(Loaded);
		if (!DataTable)
		{
			OutErrorEnvelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			if (!Loaded)
			{
				MCPEditEnvelope::SetEditError(OutErrorEnvelope, MCPEditErrors::AssetNotFound,
					FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
			}
			else
			{
				MCPEditEnvelope::SetEditError(OutErrorEnvelope, MCPEditErrors::InvalidAssetType,
					FString::Printf(TEXT("Asset is not a DataTable: %s"), *AssetPath));
			}
		}
		return DataTable;
	}

	bool SaveDataTablePackage(UDataTable* DataTable, FString& OutError)
	{
		UPackage* Package = DataTable->GetOutermost();
		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		bool bSaved = UPackage::SavePackage(Package, DataTable, *PackageFileName, SaveArgs);
		if (!bSaved)
		{
			OutError = TEXT("SavePackage failed");
		}
		return bSaved;
	}

	// Extract rows array from Args
	TArray<TSharedPtr<FJsonValue>> GetRowsArray(const TSharedPtr<FJsonObject>& Args)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		if (Args.IsValid() && Args->HasField(TEXT("rows")))
		{
			Result = Args->GetArrayField(TEXT("rows"));
		}
		return Result;
	}
}

// ============================================================================
// ue_get_datatable_schema
// ============================================================================
namespace GetDataTableSchema
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_datatable_schema");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get the row structure schema of a DataTable asset, including field names, types, and editability.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_datatable_schema");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			TSharedPtr<FJsonObject> Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SchemaNotAvailable,
				TEXT("DataTable has no RowStruct defined"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> Fields = MCPStructJsonConverter::BuildFieldSchemaArray(RowStruct);

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetStringField(TEXT("datatable_asset"), AssetPath);
		Envelope->SetStringField(TEXT("row_struct"), RowStruct->GetPathName());
		Envelope->SetStringField(TEXT("row_name_field"), TEXT("RowName"));
		Envelope->SetArrayField(TEXT("fields"), Fields);
		return Envelope;
	}
}

// ============================================================================
// ue_get_datatable_rows
// ============================================================================
namespace GetDataTableRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_datatable_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Read row data from a DataTable. Optionally filter by row names. Returns all rows if no filter is provided.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_datatable_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));
		TArray<FString> FilterRowNames = MCPToolExecution::GetStringArrayParam(Args, TEXT("row_names"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			TSharedPtr<FJsonObject> Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SchemaNotAvailable,
				TEXT("DataTable has no RowStruct"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> RowsArray;
		TArray<FString> Warnings;

		if (FilterRowNames.Num() > 0)
		{
			// Read specific rows
			for (const FString& RowName : FilterRowNames)
			{
				uint8* RowPtr = DataTable->FindRowUnchecked(FName(*RowName));
				if (!RowPtr)
				{
					Warnings.Add(FString::Printf(TEXT("Row '%s' not found"), *RowName));
					continue;
				}

				TSharedPtr<FJsonObject> RowJson;
				TArray<FString> Errors;
				MCPStructJsonConverter::StructToJson(RowStruct, RowPtr, RowJson, Errors);

				TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
				RowObj->SetStringField(TEXT("row_name"), RowName);
				RowObj->SetObjectField(TEXT("data"), RowJson);
				RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
			}
		}
		else
		{
			// Read all rows
			const TMap<FName, uint8*>& RowMap = DataTable->GetRowMap();
			for (const auto& Pair : RowMap)
			{
				TSharedPtr<FJsonObject> RowJson;
				TArray<FString> Errors;
				MCPStructJsonConverter::StructToJson(RowStruct, Pair.Value, RowJson, Errors);

				TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
				RowObj->SetStringField(TEXT("row_name"), Pair.Key.ToString());
				RowObj->SetObjectField(TEXT("data"), RowJson);
				RowsArray.Add(MakeShared<FJsonValueObject>(RowObj));
			}
		}

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetStringField(TEXT("datatable_asset"), AssetPath);
		Envelope->SetArrayField(TEXT("rows"), RowsArray);

		if (Warnings.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> WarningValues;
			for (const FString& W : Warnings)
			{
				WarningValues.Add(MakeShared<FJsonValueString>(W));
			}
			Envelope->SetArrayField(TEXT("warnings"), WarningValues);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_list_datatable_row_names
// ============================================================================
namespace ListDataTableRowNames
{
	static constexpr const TCHAR* ToolName = TEXT("ue_list_datatable_row_names");
	static constexpr const TCHAR* ToolDescription =
		TEXT("List all row names in a DataTable without reading the row data.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_list_datatable_row_names");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		TArray<FName> RowNames = DataTable->GetRowNames();
		TArray<TSharedPtr<FJsonValue>> NameArray;
		NameArray.Reserve(RowNames.Num());
		for (const FName& Name : RowNames)
		{
			NameArray.Add(MakeShared<FJsonValueString>(Name.ToString()));
		}

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetArrayField(TEXT("row_names"), NameArray);
		return Envelope;
	}
}

// ============================================================================
// ue_validate_datatable_rows
// ============================================================================
namespace ValidateDataTableRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_validate_datatable_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Validate row payloads against a DataTable's row struct schema without writing any data.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_validate_datatable_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			TSharedPtr<FJsonObject> Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SchemaNotAvailable,
				TEXT("DataTable has no RowStruct"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> Rows = GetRowsArray(Args);
		TArray<TSharedPtr<FJsonValue>> ValidationResults;
		bool bAllValid = true;

		for (const TSharedPtr<FJsonValue>& RowVal : Rows)
		{
			const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
			FString RowName = RowObj->GetStringField(TEXT("row_name"));
			const TSharedPtr<FJsonObject>* DataPtr = nullptr;
			RowObj->TryGetObjectField(TEXT("data"), DataPtr);

			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetStringField(TEXT("row_name"), RowName);

			if (!DataPtr || !(*DataPtr).IsValid())
			{
				ResultObj->SetBoolField(TEXT("valid"), false);
				TArray<TSharedPtr<FJsonValue>> ErrArr;
				TSharedPtr<FJsonObject> ErrObj = MakeShared<FJsonObject>();
				ErrObj->SetStringField(TEXT("code"), TEXT("MissingData"));
				ErrObj->SetStringField(TEXT("field"), TEXT("data"));
				ErrObj->SetStringField(TEXT("message"), TEXT("Row data is missing or null"));
				ErrArr.Add(MakeShared<FJsonValueObject>(ErrObj));
				ResultObj->SetArrayField(TEXT("errors"), ErrArr);
				bAllValid = false;
			}
			else
			{
				TArray<TSharedPtr<FJsonObject>> Errors = MCPStructJsonConverter::ValidateJsonAgainstStruct(*DataPtr, RowStruct);
				bool bRowValid = (Errors.Num() == 0);
				if (!bRowValid) bAllValid = false;

				ResultObj->SetBoolField(TEXT("valid"), bRowValid);
				TArray<TSharedPtr<FJsonValue>> ErrorValues;
				for (const auto& Err : Errors)
				{
					ErrorValues.Add(MakeShared<FJsonValueObject>(Err));
				}
				ResultObj->SetArrayField(TEXT("errors"), ErrorValues);
			}

			ValidationResults.Add(MakeShared<FJsonValueObject>(ResultObj));
		}

		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetBoolField(TEXT("ok"), true);
		Envelope->SetBoolField(TEXT("valid"), bAllValid);
		Envelope->SetArrayField(TEXT("validation_results"), ValidationResults);
		return Envelope;
	}
}

// ============================================================================
// ue_create_datatable
// ============================================================================
namespace CreateDataTable
{
	static constexpr const TCHAR* ToolName = TEXT("ue_create_datatable");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Create a new empty DataTable asset with a specified row struct.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_create_datatable");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString PackagePath = MCPToolExecution::GetStringParam(Args, TEXT("package_path"));
		FString AssetName = MCPToolExecution::GetStringParam(Args, TEXT("asset_name"));
		FString RowStructPath = MCPToolExecution::GetStringParam(Args, TEXT("row_struct"));

		// Validate paths
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
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::RowNameConflict,
				FString::Printf(TEXT("Asset already exists: %s/%s"), *PackagePath, *AssetName));
			return Envelope;
		}

		// Find row struct
		UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *RowStructPath);
		if (!RowStruct)
		{
			// Try loading via soft path
			FSoftObjectPath(RowStructPath).TryLoad();
			RowStruct = FindObject<UScriptStruct>(nullptr, *RowStructPath);
		}

		if (!RowStruct)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::UnsupportedRowStruct,
				FString::Printf(TEXT("Could not find row struct: %s"), *RowStructPath));
			return Envelope;
		}

#if WITH_EDITOR
		FString FullPath = PackagePath / AssetName;

		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Create DataTable")));

		UPackage* Package = CreatePackage(*FullPath);
		if (!Package)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("Failed to create package"));
			return Envelope;
		}

		UDataTable* NewTable = NewObject<UDataTable>(Package, *AssetName, RF_Public | RF_Standalone);
		NewTable->RowStruct = RowStruct;
		FAssetRegistryModule::AssetCreated(NewTable);
		NewTable->MarkPackageDirty();

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false, {}, {}, false, true);
		Envelope->SetStringField(TEXT("datatable_asset"), FullPath + TEXT(".") + AssetName);
		Envelope->SetStringField(TEXT("row_struct"), RowStruct->GetPathName());
		return Envelope;
#else
		Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
		MCPEditEnvelope::SetEditError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_create_datatable requires an editor build (WITH_EDITOR)"));
		return Envelope;
#endif
	}
}

// ============================================================================
// ue_upsert_datatable_rows
// ============================================================================
namespace UpsertDataTableRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_upsert_datatable_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Insert or update rows in a DataTable. Existing rows are patched; new rows are added. Supports dry_run and auto-save.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_upsert_datatable_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SchemaNotAvailable,
				TEXT("DataTable has no RowStruct"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> Rows = GetRowsArray(Args);
		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Warnings;
		TArray<FString> Errors;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Upsert DataTable Rows")), !bDryRun);
#endif

		for (const TSharedPtr<FJsonValue>& RowVal : Rows)
		{
			const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
			if (!RowObj.IsValid()) continue;

			FString RowName = RowObj->GetStringField(TEXT("row_name"));
			const TSharedPtr<FJsonObject>* DataPtr = nullptr;
			RowObj->TryGetObjectField(TEXT("data"), DataPtr);

			if (!DataPtr || !(*DataPtr).IsValid())
			{
				Errors.Add(FString::Printf(TEXT("Row '%s': missing data"), *RowName));
				continue;
			}

			uint8* ExistingRow = DataTable->FindRowUnchecked(FName(*RowName));

			if (ExistingRow)
			{
				// UPDATE existing row
				TSharedPtr<FJsonObject> OldJson;
				TArray<FString> SerErrors;
				MCPStructJsonConverter::StructToJson(RowStruct, ExistingRow, OldJson, SerErrors);

				if (!bDryRun)
				{
					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, ExistingRow, WriteErrors, true);
					if (WriteErrors.Num() > 0)
					{
						for (const FString& E : WriteErrors)
						{
							Errors.Add(FString::Printf(TEXT("Row '%s': %s"), *RowName, *E));
						}
					}
				}

				// Build change descriptor
				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("update_row"));
				Change->SetStringField(TEXT("row_name"), RowName);

				// Compute changed fields
				TSharedPtr<FJsonObject> ChangedFields = MakeShared<FJsonObject>();
				for (const auto& Pair : (*DataPtr)->Values)
				{
					ChangedFields->SetField(Pair.Key, Pair.Value);
				}
				Change->SetObjectField(TEXT("changed_fields"), ChangedFields);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
			else
			{
				// INSERT new row
				if (!bDryRun)
				{
					// Allocate and initialize a new row
					uint8* NewRowData = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
					RowStruct->InitializeStruct(NewRowData);

					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, NewRowData, WriteErrors, false);
					if (WriteErrors.Num() > 0)
					{
						for (const FString& E : WriteErrors)
						{
							Errors.Add(FString::Printf(TEXT("Row '%s': %s"), *RowName, *E));
						}
					}

					DataTable->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(NewRowData));

					RowStruct->DestroyStruct(NewRowData);
					FMemory::Free(NewRowData);
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("add_row"));
				Change->SetStringField(TEXT("row_name"), RowName);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
		}

		if (!bDryRun)
		{
			DataTable->MarkPackageDirty();

			if (bSave)
			{
				FString SaveError;
				if (!SaveDataTablePackage(DataTable, SaveError))
				{
					Warnings.Add(FString::Printf(TEXT("Auto-save failed: %s"), *SaveError));
				}
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Errors.Num() == 0, bDryRun, Changes, Warnings, false, !bDryRun && !bSave);

		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorValues;
			for (const FString& E : Errors)
			{
				ErrorValues.Add(MakeShared<FJsonValueString>(E));
			}
			Envelope->SetArrayField(TEXT("errors"), ErrorValues);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_delete_datatable_rows
// ============================================================================
namespace DeleteDataTableRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_delete_datatable_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Delete one or more rows from a DataTable by row name. Supports dry_run.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_delete_datatable_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));
		TArray<FString> RowNames = MCPToolExecution::GetStringArrayParam(Args, TEXT("row_names"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Warnings;

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Delete DataTable Rows")), !bDryRun);
#endif

		for (const FString& RowName : RowNames)
		{
			uint8* ExistingRow = DataTable->FindRowUnchecked(FName(*RowName));
			if (!ExistingRow)
			{
				Warnings.Add(FString::Printf(TEXT("Row '%s' not found, skipped"), *RowName));
				continue;
			}

			if (!bDryRun)
			{
				DataTable->RemoveRow(FName(*RowName));
			}

			TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
			Change->SetStringField(TEXT("op"), TEXT("delete_row"));
			Change->SetStringField(TEXT("row_name"), RowName);
			Changes.Add(MakeShared<FJsonValueObject>(Change));
		}

		if (!bDryRun && Changes.Num() > 0)
		{
			DataTable->MarkPackageDirty();

			if (bSave)
			{
				FString SaveError;
				if (!SaveDataTablePackage(DataTable, SaveError))
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
// ue_replace_datatable_rows
// ============================================================================
namespace ReplaceDataTableRows
{
	static constexpr const TCHAR* ToolName = TEXT("ue_replace_datatable_rows");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Replace all rows in a DataTable. Rows not in the input are deleted, new rows are added, existing rows are updated.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_replace_datatable_rows");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SchemaNotAvailable,
				TEXT("DataTable has no RowStruct"));
			return Envelope;
		}

		TArray<TSharedPtr<FJsonValue>> InputRows = GetRowsArray(Args);
		TArray<TSharedPtr<FJsonValue>> Changes;
		TArray<FString> Warnings;
		TArray<FString> Errors;

		// Build target row name set
		TSet<FString> TargetRowNames;
		for (const auto& RowVal : InputRows)
		{
			const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
			if (RowObj.IsValid())
			{
				TargetRowNames.Add(RowObj->GetStringField(TEXT("row_name")));
			}
		}

		// Get current row names
		TArray<FName> CurrentRowNames = DataTable->GetRowNames();

#if WITH_EDITOR
		FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Replace DataTable Rows")), !bDryRun);
#endif

		// Delete rows not in target
		for (const FName& CurrentName : CurrentRowNames)
		{
			FString NameStr = CurrentName.ToString();
			if (!TargetRowNames.Contains(NameStr))
			{
				if (!bDryRun)
				{
					DataTable->RemoveRow(CurrentName);
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("delete_row"));
				Change->SetStringField(TEXT("row_name"), NameStr);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
		}

		// Upsert target rows
		for (const auto& RowVal : InputRows)
		{
			const TSharedPtr<FJsonObject>& RowObj = RowVal->AsObject();
			if (!RowObj.IsValid()) continue;

			FString RowName = RowObj->GetStringField(TEXT("row_name"));
			const TSharedPtr<FJsonObject>* DataPtr = nullptr;
			RowObj->TryGetObjectField(TEXT("data"), DataPtr);

			if (!DataPtr || !(*DataPtr).IsValid())
			{
				Errors.Add(FString::Printf(TEXT("Row '%s': missing data"), *RowName));
				continue;
			}

			uint8* ExistingRow = DataTable->FindRowUnchecked(FName(*RowName));

			if (ExistingRow)
			{
				if (!bDryRun)
				{
					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, ExistingRow, WriteErrors, true);
					for (const FString& E : WriteErrors) Errors.Add(FString::Printf(TEXT("Row '%s': %s"), *RowName, *E));
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("update_row"));
				Change->SetStringField(TEXT("row_name"), RowName);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
			else
			{
				if (!bDryRun)
				{
					uint8* NewRowData = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
					RowStruct->InitializeStruct(NewRowData);

					TArray<FString> WriteErrors;
					MCPStructJsonConverter::JsonToStruct(*DataPtr, RowStruct, NewRowData, WriteErrors, false);
					for (const FString& E : WriteErrors) Errors.Add(FString::Printf(TEXT("Row '%s': %s"), *RowName, *E));

					DataTable->AddRow(FName(*RowName), *reinterpret_cast<FTableRowBase*>(NewRowData));

					RowStruct->DestroyStruct(NewRowData);
					FMemory::Free(NewRowData);
				}

				TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
				Change->SetStringField(TEXT("op"), TEXT("add_row"));
				Change->SetStringField(TEXT("row_name"), RowName);
				Changes.Add(MakeShared<FJsonValueObject>(Change));
			}
		}

		if (!bDryRun)
		{
			DataTable->MarkPackageDirty();

			if (bSave)
			{
				FString SaveError;
				if (!SaveDataTablePackage(DataTable, SaveError))
				{
					Warnings.Add(FString::Printf(TEXT("Auto-save failed: %s"), *SaveError));
				}
			}
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(Errors.Num() == 0, bDryRun, Changes, Warnings, false, !bDryRun && !bSave);

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
// ue_rename_datatable_row
// ============================================================================
namespace RenameDataTableRow
{
	static constexpr const TCHAR* ToolName = TEXT("ue_rename_datatable_row");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Rename a row in a DataTable by copying its data to a new row name and deleting the old one.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_rename_datatable_row");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));
		FString OldRowName = MCPToolExecution::GetStringParam(Args, TEXT("old_row_name"));
		FString NewRowName = MCPToolExecution::GetStringParam(Args, TEXT("new_row_name"));
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		const UScriptStruct* RowStruct = DataTable->GetRowStruct();
		if (!RowStruct)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SchemaNotAvailable,
				TEXT("DataTable has no RowStruct"));
			return Envelope;
		}

		// Check old row exists
		uint8* OldRowPtr = DataTable->FindRowUnchecked(FName(*OldRowName));
		if (!OldRowPtr)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::KeyNotFound,
				FString::Printf(TEXT("Row '%s' not found"), *OldRowName));
			return Envelope;
		}

		// Check new row doesn't exist
		uint8* NewRowPtr = DataTable->FindRowUnchecked(FName(*NewRowName));
		if (NewRowPtr)
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::RowNameConflict,
				FString::Printf(TEXT("Row '%s' already exists"), *NewRowName));
			return Envelope;
		}

		if (!bDryRun)
		{
#if WITH_EDITOR
			FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Rename DataTable Row")));
#endif

			// Copy row data
			uint8* CopyData = static_cast<uint8*>(FMemory::Malloc(RowStruct->GetStructureSize()));
			RowStruct->InitializeStruct(CopyData);
			RowStruct->CopyScriptStruct(CopyData, OldRowPtr);

			// Add new row and remove old
			DataTable->AddRow(FName(*NewRowName), *reinterpret_cast<FTableRowBase*>(CopyData));
			DataTable->RemoveRow(FName(*OldRowName));

			RowStruct->DestroyStruct(CopyData);
			FMemory::Free(CopyData);

			DataTable->MarkPackageDirty();

			if (bSave)
			{
				FString SaveError;
				if (!SaveDataTablePackage(DataTable, SaveError))
				{
					UE_LOG(LogMCPDataTable, Warning, TEXT("Auto-save failed: %s"), *SaveError);
				}
			}
		}

		TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
		Change->SetStringField(TEXT("op"), TEXT("rename_row"));
		Change->SetStringField(TEXT("old_row_name"), OldRowName);
		Change->SetStringField(TEXT("new_row_name"), NewRowName);

		TArray<TSharedPtr<FJsonValue>> Changes;
		Changes.Add(MakeShared<FJsonValueObject>(Change));

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, bDryRun, Changes, {}, false, !bDryRun && !bSave);
		return Envelope;
	}
}

// ============================================================================
// ue_save_datatable
// ============================================================================
namespace SaveDataTable
{
	static constexpr const TCHAR* ToolName = TEXT("ue_save_datatable");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Save a DataTable asset to disk.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_save_datatable");

	TSharedPtr<FJsonObject> Execute(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (IsReadOnly(RuntimeState, Envelope)) return Envelope;

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("datatable_asset"));

		TSharedPtr<FJsonObject> ErrorEnvelope;
		UDataTable* DataTable = LoadDataTable(AssetPath, ErrorEnvelope);
		if (!DataTable) return ErrorEnvelope;

		FString SaveError;
		if (!SaveDataTablePackage(DataTable, SaveError))
		{
			Envelope = MCPEditEnvelope::MakeEditEnvelope(false, false);
			MCPEditEnvelope::SetEditError(Envelope, MCPEditErrors::SaveFailed, SaveError);
			return Envelope;
		}

		Envelope = MCPEditEnvelope::MakeEditEnvelope(true, false);
		Envelope->SetStringField(TEXT("datatable_asset"), AssetPath);
		Envelope->SetStringField(TEXT("message"), TEXT("DataTable saved successfully"));
		return Envelope;
	}
}

// ============================================================================
// RegisterAll
// ============================================================================

void MCPTool_DataTable::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
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

	RegisterGameThreadTool(GetDataTableSchema::ToolName, GetDataTableSchema::ToolDescription, GetDataTableSchema::SchemaDefName, GetDataTableSchema::Execute);
	RegisterGameThreadTool(GetDataTableRows::ToolName, GetDataTableRows::ToolDescription, GetDataTableRows::SchemaDefName, GetDataTableRows::Execute);
	RegisterGameThreadTool(ListDataTableRowNames::ToolName, ListDataTableRowNames::ToolDescription, ListDataTableRowNames::SchemaDefName, ListDataTableRowNames::Execute);
	RegisterGameThreadTool(ValidateDataTableRows::ToolName, ValidateDataTableRows::ToolDescription, ValidateDataTableRows::SchemaDefName, ValidateDataTableRows::Execute);
	RegisterGameThreadTool(CreateDataTable::ToolName, CreateDataTable::ToolDescription, CreateDataTable::SchemaDefName, CreateDataTable::Execute);
	RegisterGameThreadTool(UpsertDataTableRows::ToolName, UpsertDataTableRows::ToolDescription, UpsertDataTableRows::SchemaDefName, UpsertDataTableRows::Execute);
	RegisterGameThreadTool(DeleteDataTableRows::ToolName, DeleteDataTableRows::ToolDescription, DeleteDataTableRows::SchemaDefName, DeleteDataTableRows::Execute);
	RegisterGameThreadTool(ReplaceDataTableRows::ToolName, ReplaceDataTableRows::ToolDescription, ReplaceDataTableRows::SchemaDefName, ReplaceDataTableRows::Execute);
	RegisterGameThreadTool(RenameDataTableRow::ToolName, RenameDataTableRow::ToolDescription, RenameDataTableRow::SchemaDefName, RenameDataTableRow::Execute);
	RegisterGameThreadTool(SaveDataTable::ToolName, SaveDataTable::ToolDescription, SaveDataTable::SchemaDefName, SaveDataTable::Execute);
}
