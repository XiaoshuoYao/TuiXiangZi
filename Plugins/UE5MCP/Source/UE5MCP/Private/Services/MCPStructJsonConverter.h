// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Bidirectional converter between JSON and UStruct/UClass property data.
 * Supports all common UE property types including math structs, enums,
 * object references, arrays, and nested structs.
 * All functions must be called on the GameThread.
 */
namespace MCPStructJsonConverter
{
	// ===== UStruct -> JSON =====

	/**
	 * Serialize a UStruct instance to a JSON object.
	 * @param StructDefinition  UScriptStruct* describing the struct type
	 * @param StructData        Pointer to the struct instance memory
	 * @param OutJson           Output JSON object
	 * @param OutErrors         Errors encountered during serialization
	 * @return                  true if successful
	 */
	bool StructToJson(
		const UScriptStruct* StructDefinition,
		const void* StructData,
		TSharedPtr<FJsonObject>& OutJson,
		TArray<FString>& OutErrors);

	// ===== JSON -> UStruct =====

	/**
	 * Write a JSON object into an already-allocated UStruct instance.
	 * @param JsonObject        Input JSON
	 * @param StructDefinition  UScriptStruct*
	 * @param StructData        Pointer to allocated struct memory
	 * @param OutErrors         Errors encountered during write
	 * @param bPartialPatch     true = only write fields present in JSON (patch mode)
	 *                          false = full write, missing fields reported as errors
	 * @return                  true if successful
	 */
	bool JsonToStruct(
		const TSharedPtr<FJsonObject>& JsonObject,
		const UScriptStruct* StructDefinition,
		void* StructData,
		TArray<FString>& OutErrors,
		bool bPartialPatch = false);

	// ===== Schema Generation =====

	/**
	 * Generate a field schema JSON array from a UScriptStruct.
	 * Each element: { name, type, editable, enum_values?, allowed_base_class? }
	 */
	TArray<TSharedPtr<FJsonValue>> BuildFieldSchemaArray(
		const UScriptStruct* StructDefinition);

	/**
	 * Generate a field schema JSON array from a UClass (for DataAsset).
	 * Skips properties from UObject/UDataAsset base classes.
	 * Only includes CPF_Edit or CPF_BlueprintVisible properties.
	 */
	TArray<TSharedPtr<FJsonValue>> BuildFieldSchemaArrayForClass(
		const UClass* Class);

	// ===== Validation =====

	/**
	 * Validate a JSON payload against a struct schema without writing.
	 * @return Array of error objects (empty = passed). Each: { code, field, message }
	 */
	TArray<TSharedPtr<FJsonObject>> ValidateJsonAgainstStruct(
		const TSharedPtr<FJsonObject>& JsonObject,
		const UScriptStruct* StructDefinition);

	// ===== Single Property Helpers =====

	/**
	 * Serialize a single FProperty value to a FJsonValue.
	 * @param Property  The property descriptor
	 * @param ValuePtr  Direct pointer to the property value (NOT container pointer)
	 */
	TSharedPtr<FJsonValue> PropertyToJsonValue(
		const FProperty* Property,
		const void* ValuePtr);

	/**
	 * Write a FJsonValue into a single FProperty.
	 * @param JsonValue  The JSON value to write
	 * @param Property   The property descriptor
	 * @param ValuePtr   Direct pointer to the property value (NOT container pointer)
	 * @param OutErrors  Errors encountered
	 * @return           true if successful
	 */
	bool JsonValueToProperty(
		const TSharedPtr<FJsonValue>& JsonValue,
		const FProperty* Property,
		void* ValuePtr,
		TArray<FString>& OutErrors);

	/**
	 * Map an FProperty to a simplified type string.
	 * e.g. "bool", "int32", "float", "string", "enum", "vector", "struct", etc.
	 */
	FString MapPropertyTypeString(const FProperty* Property);
}
