// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_BlueprintEdit.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "MCPToolRegistry.h"
#include "Edit/MCPEditEnvelope.h"
#include "Edit/MCPEditErrors.h"
#include "Edit/MCPPropertyPatchApplier.h"
#include "Reflection/MCPReflectionCore.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/SavePackage.h"
#include "ScopedTransaction.h"

// ============================================================================
// Helpers
// ============================================================================
namespace
{
	/** Load a blueprint by asset path. Returns nullptr on failure. */
	UBlueprint* LoadBlueprintByPath(const FString& BpPath)
	{
		return LoadObject<UBlueprint>(nullptr, *BpPath);
	}

	/** Check if a property is editable for blueprint defaults. */
	bool IsPropertyEditableForDefaults(const FProperty* Property)
	{
		if (!Property) return false;
		if (!Property->HasAnyPropertyFlags(CPF_Edit)) return false;
		if (Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate)) return false;
		if (Property->HasAnyPropertyFlags(CPF_Deprecated)) return false;
		return true;
	}

	/** Check if a property is writable (editable and not const/read-only). */
	bool IsPropertyWritable(const FProperty* Property)
	{
		if (!IsPropertyEditableForDefaults(Property)) return false;
		if (Property->HasAnyPropertyFlags(CPF_EditConst)) return false;
		if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly)) return false;
		return true;
	}

	/** Find an SCS node by component name. Searches both ComponentTemplate name and VariableName. */
	USCS_Node* FindSCSNodeByName(UBlueprint* BP, const FString& ComponentName)
	{
		if (!BP || !BP->SimpleConstructionScript) return nullptr;

		const TArray<USCS_Node*>& AllNodes = BP->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* Node : AllNodes)
		{
			if (!Node || !Node->ComponentTemplate) continue;

			if (Node->ComponentTemplate->GetName() == ComponentName)
			{
				return Node;
			}
			if (Node->GetVariableName().ToString() == ComponentName)
			{
				return Node;
			}
		}
		return nullptr;
	}

	/** Build a JSON change descriptor for a property patch. */
	TSharedPtr<FJsonValue> MakeChangeEntry(
		const FString& PropertyPath,
		const TSharedPtr<FJsonValue>& OldValue,
		const TSharedPtr<FJsonValue>& NewValue)
	{
		TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
		Change->SetStringField(TEXT("property"), PropertyPath);
		if (OldValue.IsValid())
		{
			Change->SetField(TEXT("old_value"), OldValue);
		}
		if (NewValue.IsValid())
		{
			Change->SetField(TEXT("new_value"), NewValue);
		}
		return MakeShared<FJsonValueObject>(Change);
	}

	/** Save a package to disk. Returns true on success. */
	bool SaveAssetPackage(UObject* Asset)
	{
		if (!Asset) return false;
		UPackage* Package = Asset->GetOutermost();
		if (!Package) return false;

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		FString PackageFilename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			return false;
		}
		return UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs);
	}
}

// ============================================================================
// ue_get_blueprint_edit_schema (Task 10)
// ============================================================================
namespace BlueprintEditSchema
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_blueprint_edit_schema");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Returns the editable property schema for a Blueprint class, including class defaults and component templates.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_blueprint_edit_schema");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("blueprint_asset"));

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.edit_schema") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: blueprint_asset"));
			return Envelope;
		}

		UBlueprint* BP = LoadBlueprintByPath(AssetPath);
		if (!BP)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPEditErrors::BlueprintNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
			return Envelope;
		}

		UClass* GenClass = BP->GeneratedClass;
		if (!GenClass)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPEditErrors::CompileFailed,
				TEXT("Blueprint has no GeneratedClass. It may need to be compiled."));
			return Envelope;
		}

		Envelope->SetStringField(TEXT("blueprint_asset"), AssetPath);

		// --- Class defaults properties ---
		TArray<TSharedPtr<FJsonValue>> ClassDefaultsArray;
		for (TFieldIterator<FProperty> PropIt(GenClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!IsPropertyEditableForDefaults(Property)) continue;

			TSharedPtr<FJsonObject> PropDesc = MCPReflection::BuildPropertyDesc(Property, true);
			if (PropDesc.IsValid())
			{
				bool bEditable = IsPropertyWritable(Property);
				PropDesc->SetBoolField(TEXT("editable"), bEditable);
				ClassDefaultsArray.Add(MakeShared<FJsonValueObject>(PropDesc));
			}
		}
		Envelope->SetArrayField(TEXT("class_defaults"), ClassDefaultsArray);

		// --- Component templates ---
		TArray<TSharedPtr<FJsonValue>> ComponentsArray;
		if (BP->SimpleConstructionScript)
		{
			const TArray<USCS_Node*>& AllNodes = BP->SimpleConstructionScript->GetAllNodes();
			for (USCS_Node* Node : AllNodes)
			{
				if (!Node || !Node->ComponentTemplate) continue;

				TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
				CompObj->SetStringField(TEXT("component_name"), Node->ComponentTemplate->GetName());
				CompObj->SetStringField(TEXT("variable_name"), Node->GetVariableName().ToString());
				CompObj->SetStringField(TEXT("component_class"), Node->ComponentTemplate->GetClass()->GetPathName());

				TArray<TSharedPtr<FJsonValue>> CompPropsArray;
				for (TFieldIterator<FProperty> PropIt(Node->ComponentTemplate->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
				{
					FProperty* Property = *PropIt;
					if (!IsPropertyEditableForDefaults(Property)) continue;

					TSharedPtr<FJsonObject> PropDesc = MCPReflection::BuildPropertyDesc(Property, true);
					if (PropDesc.IsValid())
					{
						bool bEditable = IsPropertyWritable(Property);
						PropDesc->SetBoolField(TEXT("editable"), bEditable);
						CompPropsArray.Add(MakeShared<FJsonValueObject>(PropDesc));
					}
				}
				CompObj->SetArrayField(TEXT("properties"), CompPropsArray);
				ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
			}
		}
		Envelope->SetArrayField(TEXT("component_templates"), ComponentsArray);

		return Envelope;
	}
}

// ============================================================================
// ue_get_blueprint_defaults (Task 11)
// ============================================================================
namespace BlueprintGetDefaults
{
	static constexpr const TCHAR* ToolName = TEXT("ue_get_blueprint_defaults");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Read current default values for a Blueprint's class defaults and (optionally) component templates.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_get_blueprint_defaults");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("blueprint_asset"));
		bool bIncludeComponents = MCPToolExecution::GetBoolParam(Args, TEXT("include_components"), true);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.get_defaults") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: blueprint_asset"));
			return Envelope;
		}

		UBlueprint* BP = LoadBlueprintByPath(AssetPath);
		if (!BP)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPEditErrors::BlueprintNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
			return Envelope;
		}

		UClass* GenClass = BP->GeneratedClass;
		if (!GenClass)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPEditErrors::CompileFailed,
				TEXT("Blueprint has no GeneratedClass. It may need to be compiled."));
			return Envelope;
		}

		UObject* CDO = GenClass->GetDefaultObject();
		if (!CDO)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInternal,
				TEXT("Failed to get Class Default Object."));
			return Envelope;
		}

		Envelope->SetStringField(TEXT("blueprint_asset"), AssetPath);

		// --- Class defaults ---
		TSharedPtr<FJsonObject> ClassDefaults = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> PropIt(GenClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!IsPropertyEditableForDefaults(Property)) continue;

			TSharedPtr<FJsonValue> Value = MCPPropertyPatchApplier::ReadPropertyAsJson(Property, CDO);
			if (Value.IsValid())
			{
				ClassDefaults->SetField(Property->GetName(), Value);
			}
		}
		Envelope->SetObjectField(TEXT("class_defaults"), ClassDefaults);

		// --- Component defaults ---
		if (bIncludeComponents && BP->SimpleConstructionScript)
		{
			TSharedPtr<FJsonObject> ComponentDefaults = MakeShared<FJsonObject>();

			const TArray<USCS_Node*>& AllNodes = BP->SimpleConstructionScript->GetAllNodes();
			for (USCS_Node* Node : AllNodes)
			{
				if (!Node || !Node->ComponentTemplate) continue;

				TSharedPtr<FJsonObject> CompProps = MakeShared<FJsonObject>();
				UObject* Template = Node->ComponentTemplate;

				for (TFieldIterator<FProperty> PropIt(Template->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
				{
					FProperty* Property = *PropIt;
					if (!IsPropertyEditableForDefaults(Property)) continue;

					TSharedPtr<FJsonValue> Value = MCPPropertyPatchApplier::ReadPropertyAsJson(Property, Template);
					if (Value.IsValid())
					{
						CompProps->SetField(Property->GetName(), Value);
					}
				}

				ComponentDefaults->SetObjectField(Node->ComponentTemplate->GetName(), CompProps);
			}
			Envelope->SetObjectField(TEXT("component_defaults"), ComponentDefaults);
		}

		return Envelope;
	}
}

// ============================================================================
// ue_set_blueprint_defaults (Task 12)
// ============================================================================
namespace BlueprintSetDefaults
{
	static constexpr const TCHAR* ToolName = TEXT("ue_set_blueprint_defaults");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Set default property values on a Blueprint's Class Default Object. Supports dry-run, compile, and save.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_set_blueprint_defaults");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("blueprint_asset"));
		bool bCompile = MCPToolExecution::GetBoolParam(Args, TEXT("compile"), true);
		bool bSave = MCPToolExecution::GetBoolParam(Args, TEXT("save"), false);
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);

		// Extract patch array
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Patches;
		const TArray<TSharedPtr<FJsonValue>>* PatchArray = nullptr;
		if (Args->TryGetArrayField(TEXT("patch"), PatchArray))
		{
			for (const TSharedPtr<FJsonValue>& PatchVal : *PatchArray)
			{
				if (!PatchVal.IsValid()) continue;
				const TSharedPtr<FJsonObject>* PatchObj = nullptr;
				if (PatchVal->TryGetObject(PatchObj))
				{
					FString PropPath = (*PatchObj)->GetStringField(TEXT("property_path"));
					TSharedPtr<FJsonValue> Value;
					if ((*PatchObj)->HasField(TEXT("value")))
					{
						Value = (*PatchObj)->TryGetField(TEXT("value"));
					}
					if (!PropPath.IsEmpty())
					{
						Patches.Add(TPair<FString, TSharedPtr<FJsonValue>>(PropPath, Value));
					}
				}
			}
		}

		if (AssetPath.IsEmpty())
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: blueprint_asset"));
			return Env;
		}

		if (Patches.Num() == 0)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing or empty patch array."));
			return Env;
		}

		// Read-only check
		if (!bDryRun && Snap.bReadOnly)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorReadOnly,
				TEXT("Server is in read-only mode. Mutations are disabled."));
			return Env;
		}

		UBlueprint* BP = LoadBlueprintByPath(AssetPath);
		if (!BP)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPEditErrors::BlueprintNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
			return Env;
		}

		UClass* GenClass = BP->GeneratedClass;
		if (!GenClass)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPEditErrors::CompileFailed,
				TEXT("Blueprint has no GeneratedClass."));
			return Env;
		}

		UObject* CDO = GenClass->GetDefaultObject();
		if (!CDO)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorInternal,
				TEXT("Failed to get Class Default Object."));
			return Env;
		}

		// Validate all patches first
		TArray<FString> Warnings;
		TArray<TSharedPtr<FJsonValue>> Changes;
		bool bAllOk = true;

		for (const auto& PatchEntry : Patches)
		{
			const FString& PropertyPath = PatchEntry.Key;
			const TSharedPtr<FJsonValue>& NewJsonValue = PatchEntry.Value;

			// Resolve property
			MCPPropertyPatchApplier::FResolvedProperty Resolved =
				MCPPropertyPatchApplier::ResolvePropertyPath(CDO, PropertyPath);

			if (!Resolved.IsValid())
			{
				TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
				MCPEditEnvelope::SetEditError(Env, MCPEditErrors::PropertyNotFound,
					FString::Printf(TEXT("Property not found: %s — %s"), *PropertyPath, *Resolved.ErrorMessage));
				return Env;
			}

			// Check editability
			if (!IsPropertyWritable(Resolved.Property))
			{
				TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
				MCPEditEnvelope::SetEditError(Env, MCPEditErrors::PropertyNotEditable,
					FString::Printf(TEXT("Property is not editable: %s"), *PropertyPath));
				return Env;
			}

			if (bDryRun)
			{
				// Read current value for preview
				TSharedPtr<FJsonValue> OldValue = MCPPropertyPatchApplier::ReadPropertyAsJson(
					Resolved.Property, Resolved.ContainerPtr);
				Changes.Add(MakeChangeEntry(PropertyPath, OldValue, NewJsonValue));
			}
		}

		if (bDryRun)
		{
			return MCPEditEnvelope::MakeEditEnvelope(true, true, Changes, Warnings, bCompile, bSave);
		}

		// --- Apply patches ---
		{
			FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Blueprint Defaults")));
			CDO->Modify();

			for (const auto& PatchEntry : Patches)
			{
				const FString& PropertyPath = PatchEntry.Key;
				const TSharedPtr<FJsonValue>& NewJsonValue = PatchEntry.Value;

				// Read old value before applying
				MCPPropertyPatchApplier::FResolvedProperty Resolved =
					MCPPropertyPatchApplier::ResolvePropertyPath(CDO, PropertyPath);
				TSharedPtr<FJsonValue> OldValue = MCPPropertyPatchApplier::ReadPropertyAsJson(
					Resolved.Property, Resolved.ContainerPtr);

				MCPPropertyPatchApplier::FPatchResult Result =
					MCPPropertyPatchApplier::ApplyPatch(CDO, PropertyPath, NewJsonValue);

				if (Result.bSuccess)
				{
					Changes.Add(MakeChangeEntry(PropertyPath, OldValue, Result.NewValue));
				}
				else
				{
					TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, false, Changes, Warnings);
					MCPEditEnvelope::SetEditError(Env, Result.ErrorCode, Result.ErrorMessage);
					return Env;
				}
			}

			BP->Modify();
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		}

		// Compile if requested
		if (bCompile)
		{
			FKismetEditorUtilities::CompileBlueprint(BP);
		}

		// Save if requested
		bool bSaved = false;
		if (bSave)
		{
			bSaved = SaveAssetPackage(BP);
			if (!bSaved)
			{
				Warnings.Add(TEXT("Failed to save package after applying changes."));
			}
		}

		return MCPEditEnvelope::MakeEditEnvelope(
			true, false, Changes, Warnings,
			!bCompile,  // needs_compile if we didn't compile
			!bSave);    // needs_save if we didn't save
	}
}

// ============================================================================
// ue_set_blueprint_component_defaults (Task 13)
// ============================================================================
namespace BlueprintSetComponentDefaults
{
	static constexpr const TCHAR* ToolName = TEXT("ue_set_blueprint_component_defaults");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Set default property values on a specific component template in a Blueprint.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_set_blueprint_component_defaults");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("blueprint_asset"));
		FString ComponentName = MCPToolExecution::GetStringParam(Args, TEXT("component_name"));
		bool bCompile = MCPToolExecution::GetBoolParam(Args, TEXT("compile"), true);
		bool bDryRun = MCPToolExecution::GetBoolParam(Args, TEXT("dry_run"), false);

		// Extract patch array
		TArray<TPair<FString, TSharedPtr<FJsonValue>>> Patches;
		const TArray<TSharedPtr<FJsonValue>>* PatchArray = nullptr;
		if (Args->TryGetArrayField(TEXT("patch"), PatchArray))
		{
			for (const TSharedPtr<FJsonValue>& PatchVal : *PatchArray)
			{
				if (!PatchVal.IsValid()) continue;
				const TSharedPtr<FJsonObject>* PatchObj = nullptr;
				if (PatchVal->TryGetObject(PatchObj))
				{
					FString PropPath = (*PatchObj)->GetStringField(TEXT("property_path"));
					TSharedPtr<FJsonValue> Value;
					if ((*PatchObj)->HasField(TEXT("value")))
					{
						Value = (*PatchObj)->TryGetField(TEXT("value"));
					}
					if (!PropPath.IsEmpty())
					{
						Patches.Add(TPair<FString, TSharedPtr<FJsonValue>>(PropPath, Value));
					}
				}
			}
		}

		if (AssetPath.IsEmpty())
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: blueprint_asset"));
			return Env;
		}

		if (ComponentName.IsEmpty())
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: component_name"));
			return Env;
		}

		if (Patches.Num() == 0)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing or empty patch array."));
			return Env;
		}

		// Read-only check
		if (!bDryRun && Snap.bReadOnly)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPToolExecution::ErrorReadOnly,
				TEXT("Server is in read-only mode. Mutations are disabled."));
			return Env;
		}

		UBlueprint* BP = LoadBlueprintByPath(AssetPath);
		if (!BP)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPEditErrors::BlueprintNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
			return Env;
		}

		// Find component
		USCS_Node* SCSNode = FindSCSNodeByName(BP, ComponentName);
		if (!SCSNode || !SCSNode->ComponentTemplate)
		{
			TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
			MCPEditEnvelope::SetEditError(Env, MCPEditErrors::ComponentNotFound,
				FString::Printf(TEXT("Component not found: %s"), *ComponentName));
			return Env;
		}

		UObject* Template = SCSNode->ComponentTemplate;

		// Validate all patches
		TArray<FString> Warnings;
		TArray<TSharedPtr<FJsonValue>> Changes;

		for (const auto& PatchEntry : Patches)
		{
			const FString& PropertyPath = PatchEntry.Key;
			const TSharedPtr<FJsonValue>& NewJsonValue = PatchEntry.Value;

			MCPPropertyPatchApplier::FResolvedProperty Resolved =
				MCPPropertyPatchApplier::ResolvePropertyPath(Template, PropertyPath);

			if (!Resolved.IsValid())
			{
				TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
				MCPEditEnvelope::SetEditError(Env, MCPEditErrors::PropertyNotFound,
					FString::Printf(TEXT("Property not found on component '%s': %s — %s"),
						*ComponentName, *PropertyPath, *Resolved.ErrorMessage));
				return Env;
			}

			if (!IsPropertyWritable(Resolved.Property))
			{
				TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, bDryRun);
				MCPEditEnvelope::SetEditError(Env, MCPEditErrors::PropertyNotEditable,
					FString::Printf(TEXT("Property is not editable: %s"), *PropertyPath));
				return Env;
			}

			if (bDryRun)
			{
				TSharedPtr<FJsonValue> OldValue = MCPPropertyPatchApplier::ReadPropertyAsJson(
					Resolved.Property, Resolved.ContainerPtr);
				Changes.Add(MakeChangeEntry(PropertyPath, OldValue, NewJsonValue));
			}
		}

		if (bDryRun)
		{
			return MCPEditEnvelope::MakeEditEnvelope(true, true, Changes, Warnings, bCompile, true);
		}

		// --- Apply patches ---
		{
			FScopedTransaction Transaction(FText::FromString(TEXT("MCP Set Blueprint Component Defaults")));
			Template->Modify();

			for (const auto& PatchEntry : Patches)
			{
				const FString& PropertyPath = PatchEntry.Key;
				const TSharedPtr<FJsonValue>& NewJsonValue = PatchEntry.Value;

				MCPPropertyPatchApplier::FResolvedProperty Resolved =
					MCPPropertyPatchApplier::ResolvePropertyPath(Template, PropertyPath);
				TSharedPtr<FJsonValue> OldValue = MCPPropertyPatchApplier::ReadPropertyAsJson(
					Resolved.Property, Resolved.ContainerPtr);

				MCPPropertyPatchApplier::FPatchResult Result =
					MCPPropertyPatchApplier::ApplyPatch(Template, PropertyPath, NewJsonValue);

				if (Result.bSuccess)
				{
					Changes.Add(MakeChangeEntry(PropertyPath, OldValue, Result.NewValue));
				}
				else
				{
					TSharedPtr<FJsonObject> Env = MCPEditEnvelope::MakeEditEnvelope(false, false, Changes, Warnings);
					MCPEditEnvelope::SetEditError(Env, Result.ErrorCode, Result.ErrorMessage);
					return Env;
				}
			}

			BP->Modify();
			FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
		}

		// Compile if requested
		if (bCompile)
		{
			FKismetEditorUtilities::CompileBlueprint(BP);
		}

		return MCPEditEnvelope::MakeEditEnvelope(
			true, false, Changes, Warnings,
			!bCompile,  // needs_compile if we didn't compile
			true);      // needs_save: component changes always need save
	}
}

// ============================================================================
// ue_compile_blueprint (Task 14)
// ============================================================================
namespace BlueprintCompile
{
	static constexpr const TCHAR* ToolName = TEXT("ue_compile_blueprint");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Compile a Blueprint asset and report the compilation result status.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_compile_blueprint");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("blueprint_asset"));

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.compile") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: blueprint_asset"));
			return Envelope;
		}

		UBlueprint* BP = LoadBlueprintByPath(AssetPath);
		if (!BP)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPEditErrors::BlueprintNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));
			return Envelope;
		}

		// Compile
		FKismetEditorUtilities::CompileBlueprint(BP);

		// Check status
		FString StatusStr;
		bool bCompiledOk = false;
		switch (BP->Status)
		{
		case BS_Unknown:
			StatusStr = TEXT("Unknown");
			break;
		case BS_Dirty:
			StatusStr = TEXT("Dirty");
			break;
		case BS_Error:
			StatusStr = TEXT("Error");
			break;
		case BS_UpToDate:
			StatusStr = TEXT("Success");
			bCompiledOk = true;
			break;
		case BS_BeingCreated:
			StatusStr = TEXT("BeingCreated");
			break;
		case BS_UpToDateWithWarnings:
			StatusStr = TEXT("SuccessWithWarnings");
			bCompiledOk = true;
			break;
		default:
			StatusStr = TEXT("Unknown");
			break;
		}

		Envelope->SetBoolField(TEXT("ok"), bCompiledOk);
		Envelope->SetBoolField(TEXT("compiled"), true);
		Envelope->SetStringField(TEXT("status"), StatusStr);
		Envelope->SetStringField(TEXT("blueprint_asset"), AssetPath);

		// Collect compiler messages if available
		TArray<TSharedPtr<FJsonValue>> ErrorsArray;
		TArray<TSharedPtr<FJsonValue>> WarningsArray;

		// Compiler messages are collected via FCompilerResultsLog during compilation
		// and are not directly accessible from UBlueprint in this UE version.

		Envelope->SetArrayField(TEXT("errors"), ErrorsArray);
		Envelope->SetArrayField(TEXT("warnings"), WarningsArray);

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_BlueprintEdit::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
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

	RegisterGameThreadTool(BlueprintEditSchema::ToolName, BlueprintEditSchema::ToolDescription, BlueprintEditSchema::SchemaDefName, BlueprintEditSchema::ExecuteOnGameThread);
	RegisterGameThreadTool(BlueprintGetDefaults::ToolName, BlueprintGetDefaults::ToolDescription, BlueprintGetDefaults::SchemaDefName, BlueprintGetDefaults::ExecuteOnGameThread);
	RegisterGameThreadTool(BlueprintSetDefaults::ToolName, BlueprintSetDefaults::ToolDescription, BlueprintSetDefaults::SchemaDefName, BlueprintSetDefaults::ExecuteOnGameThread);
	RegisterGameThreadTool(BlueprintSetComponentDefaults::ToolName, BlueprintSetComponentDefaults::ToolDescription, BlueprintSetComponentDefaults::SchemaDefName, BlueprintSetComponentDefaults::ExecuteOnGameThread);
	RegisterGameThreadTool(BlueprintCompile::ToolName, BlueprintCompile::ToolDescription, BlueprintCompile::SchemaDefName, BlueprintCompile::ExecuteOnGameThread);
}

#else // !WITH_EDITOR

void MCPTool_BlueprintEdit::RegisterAll(FMCPToolRegistry& /*Registry*/, FMCPRuntimeState& /*RuntimeState*/, FMCPResourceStore& /*ResourceStore*/)
{
	// Blueprint edit tools are only available in Editor builds
}

#endif // WITH_EDITOR
