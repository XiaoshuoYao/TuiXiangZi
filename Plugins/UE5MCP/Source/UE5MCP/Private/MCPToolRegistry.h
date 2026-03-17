// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================
// Tool Descriptor & Registration
// ============================================================================

/** Single tool's metadata for tools/list */
struct FMCPToolDescriptor
{
	FString Name;
	FString Description;
	TSharedPtr<FJsonObject> InputSchema; // Extracted from schema file at runtime
};

/** Tool execution function: receives arguments, returns envelope JSON object */
using FMCPToolExecuteFn = TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>& Arguments)>;

/** Complete registration entry: descriptor + execute function */
struct FMCPToolRegistration
{
	FMCPToolDescriptor Descriptor;
	FMCPToolExecuteFn Execute;
};

// ============================================================================
// Envelope Helpers
// ============================================================================

namespace MCPEnvelope
{
	/** Build a base envelope JSON with common fields */
	TSharedPtr<FJsonObject> MakeEnvelope(
		const TArray<FString>& Capabilities = {},
		const TArray<FString>& Warnings = {},
		bool bEditorOnly = true);

	/** Append an error block to an existing envelope */
	void SetEnvelopeError(
		const TSharedPtr<FJsonObject>& Envelope,
		const FString& Code,
		const FString& Message);
}

// ============================================================================
// Tool Registry
// ============================================================================

class FMCPToolRegistry
{
public:
	/**
	 * Load and cache the schema JSON file.
	 * Must be called before RegisterTool if tools need inputSchema extraction.
	 */
	bool LoadSchemaFile(const FString& Path);

	/**
	 * Extract inputSchema from cached schema for a given tool definition name.
	 * Path: $defs.{ToolDefName}.allOf[1].properties.input
	 */
	TSharedPtr<FJsonObject> ExtractInputSchema(const FString& ToolDefName) const;

	/** Register a tool. Overwrites if name already exists. */
	void RegisterTool(FMCPToolRegistration&& Registration);

	/** Find a registered tool by name. Returns nullptr if not found. */
	const FMCPToolRegistration* FindTool(const FString& Name) const;

	/** Build the MCP-standard tools array for tools/list response. */
	TArray<TSharedPtr<FJsonValue>> BuildToolsListJson() const;

	/** Get count of registered tools (for diagnostics). */
	int32 Num() const { return Tools.Num(); }

private:
	TMap<FString, FMCPToolRegistration> Tools;
	TSharedPtr<FJsonObject> CachedSchema; // Full schema file
};

// ============================================================================
// Phase 4/5 Extension Points (stubs - declared here, implemented in Phase 4/5)
// ============================================================================

// Phase 4: Resolve a class name (path, short name, blueprint) to UClass*
// UClass* ResolveClass(const FString& ClassName);

// Phase 5: Build property/function descriptors and type mapping
// TSharedPtr<FJsonObject> BuildPropertyDesc(FProperty* Property, bool bIncludeMetadata);
// TSharedPtr<FJsonObject> BuildFunctionDesc(UFunction* Function);
// FString MapPropertyType(FProperty* Property);
