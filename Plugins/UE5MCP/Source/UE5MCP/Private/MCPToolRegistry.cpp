// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPToolRegistry.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPToolRegistry, Log, All);

// ============================================================================
// Envelope Helpers
// ============================================================================

namespace MCPEnvelope
{
	TSharedPtr<FJsonObject> MakeEnvelope(
		const TArray<FString>& Capabilities,
		const TArray<FString>& Warnings,
		bool bEditorOnly)
	{
		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetStringField(TEXT("schema_version"), TEXT("0.1.0"));

		// capabilities
		TArray<TSharedPtr<FJsonValue>> CapArray;
		for (const FString& Cap : Capabilities)
		{
			CapArray.Add(MakeShared<FJsonValueString>(Cap));
		}
		Envelope->SetArrayField(TEXT("capabilities"), CapArray);

		// warnings
		TArray<TSharedPtr<FJsonValue>> WarnArray;
		for (const FString& W : Warnings)
		{
			WarnArray.Add(MakeShared<FJsonValueString>(W));
		}
		Envelope->SetArrayField(TEXT("warnings"), WarnArray);

		Envelope->SetBoolField(TEXT("editor_only"), bEditorOnly);

		return Envelope;
	}

	void SetEnvelopeError(
		const TSharedPtr<FJsonObject>& Envelope,
		const FString& Code,
		const FString& Message)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetStringField(TEXT("code"), Code);
		ErrorObj->SetStringField(TEXT("message"), Message);
		Envelope->SetObjectField(TEXT("error"), ErrorObj);
	}
}

// ============================================================================
// Schema Loading
// ============================================================================

bool FMCPToolRegistry::LoadSchemaFile(const FString& Path)
{
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Path))
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Failed to load schema file: %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> ParsedJson;
	auto Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, ParsedJson) || !ParsedJson.IsValid())
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Failed to parse schema file as JSON: %s"), *Path);
		return false;
	}

	CachedSchema = ParsedJson;
	UE_LOG(LogMCPToolRegistry, Log, TEXT("Schema file loaded and cached: %s"), *Path);
	return true;
}

TSharedPtr<FJsonObject> FMCPToolRegistry::ExtractInputSchema(const FString& ToolDefName) const
{
	if (!CachedSchema.IsValid())
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Cannot extract inputSchema: schema not loaded"));
		return nullptr;
	}

	// Navigate: $defs -> {ToolDefName} -> allOf[1] -> properties -> input
	const TSharedPtr<FJsonObject>* DefsPtr = nullptr;
	if (!CachedSchema->TryGetObjectField(TEXT("$defs"), DefsPtr) || !DefsPtr)
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Schema missing $defs"));
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* ToolDefPtr = nullptr;
	if (!(*DefsPtr)->TryGetObjectField(ToolDefName, ToolDefPtr) || !ToolDefPtr)
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Schema $defs missing tool definition: %s"), *ToolDefName);
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* AllOfArray = nullptr;
	if (!(*ToolDefPtr)->TryGetArrayField(TEXT("allOf"), AllOfArray) || !AllOfArray || AllOfArray->Num() < 2)
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Tool %s: allOf missing or too short"), *ToolDefName);
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* SecondAllOf = nullptr;
	if (!(*AllOfArray)[1]->TryGetObject(SecondAllOf) || !SecondAllOf)
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Tool %s: allOf[1] is not an object"), *ToolDefName);
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* PropsPtr = nullptr;
	if (!(*SecondAllOf)->TryGetObjectField(TEXT("properties"), PropsPtr) || !PropsPtr)
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Tool %s: allOf[1].properties missing"), *ToolDefName);
		return nullptr;
	}

	const TSharedPtr<FJsonObject>* InputPtr = nullptr;
	if (!(*PropsPtr)->TryGetObjectField(TEXT("input"), InputPtr) || !InputPtr)
	{
		UE_LOG(LogMCPToolRegistry, Error, TEXT("Tool %s: allOf[1].properties.input missing"), *ToolDefName);
		return nullptr;
	}

	// Return the extracted inputSchema object (read-only usage for tools/list)
	return *InputPtr;
}

// ============================================================================
// Tool Registration & Lookup
// ============================================================================

void FMCPToolRegistry::RegisterTool(FMCPToolRegistration&& Registration)
{
	FString Name = Registration.Descriptor.Name;
	UE_LOG(LogMCPToolRegistry, Log, TEXT("Registering tool: %s"), *Name);
	Tools.Add(MoveTemp(Name), MoveTemp(Registration));
}

const FMCPToolRegistration* FMCPToolRegistry::FindTool(const FString& Name) const
{
	return Tools.Find(Name);
}

TArray<TSharedPtr<FJsonValue>> FMCPToolRegistry::BuildToolsListJson() const
{
	TArray<TSharedPtr<FJsonValue>> Result;
	Result.Reserve(Tools.Num());

	for (const auto& Pair : Tools)
	{
		const FMCPToolDescriptor& Desc = Pair.Value.Descriptor;

		TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
		ToolObj->SetStringField(TEXT("name"), Desc.Name);
		ToolObj->SetStringField(TEXT("description"), Desc.Description);

		if (Desc.InputSchema.IsValid())
		{
			ToolObj->SetObjectField(TEXT("inputSchema"), Desc.InputSchema);
		}

		Result.Add(MakeShared<FJsonValueObject>(ToolObj));
	}

	return Result;
}
