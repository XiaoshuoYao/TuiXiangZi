// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MCPToolRegistry.h"
#include "Reflection/MCPReflectionCore.h"

#include "Async/Async.h"
#include "HAL/Event.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

/**
 * ue_class_schema tool — returns reflected properties and functions of a UClass.
 *
 * Phase 4: Full reflection implementation with GameThread dispatch.
 * Phase 5: Deterministic type mapping, flags whitelist, metadata filtering.
 */
namespace MCPTool_ClassSchema
{
	static constexpr const TCHAR* ToolName = TEXT("ue_class_schema");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Returns the reflected properties and functions of a UClass. "
			 "Supports /Script/... paths, /Game/..._C blueprint classes, and short names.");

	// Schema definition name in ue_mcp_schema_fixed.json ($defs key)
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_class_schema");

	/**
	 * Execute reflection on GameThread and return the envelope.
	 * Called from the Execute lambda (HTTP worker thread).
	 */
	inline TSharedPtr<FJsonObject> ExecuteOnGameThread(const TSharedPtr<FJsonObject>& Arguments)
	{
		// --- Parameter extraction ---
		FString ClassName;
		bool bHasClassName = Arguments.IsValid() && Arguments->TryGetStringField(TEXT("class_name"), ClassName);

		if (!bHasClassName || ClassName.IsEmpty())
		{
			// Check if class_name exists but is wrong type
			if (Arguments.IsValid() && Arguments->HasField(TEXT("class_name")))
			{
				TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope({}, {}, true);
				MCPEnvelope::SetEnvelopeError(Envelope,
					TEXT("INVALID_ARGUMENT"),
					TEXT("class_name must be a string"));
				Envelope->SetStringField(TEXT("class_name"), TEXT(""));
				Envelope->SetArrayField(TEXT("properties"), {});
				Envelope->SetArrayField(TEXT("functions"), {});
				return Envelope;
			}

			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope({}, {}, true);
			MCPEnvelope::SetEnvelopeError(Envelope,
				TEXT("INVALID_ARGUMENT"),
				TEXT("Missing required parameter: class_name"));
			Envelope->SetStringField(TEXT("class_name"), TEXT(""));
			Envelope->SetArrayField(TEXT("properties"), {});
			Envelope->SetArrayField(TEXT("functions"), {});
			return Envelope;
		}

		bool bIncludeInherited = true;
		bool bIncludeFunctions = true;
		bool bIncludeMetadata = false;

		if (Arguments.IsValid())
		{
			Arguments->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);
			Arguments->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
			Arguments->TryGetBoolField(TEXT("include_metadata"), bIncludeMetadata);
		}

		// --- Resolve class ---
		UClass* Class = MCPReflection::ResolveClass(ClassName);
		if (!Class)
		{
			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope({}, {}, true);
			MCPEnvelope::SetEnvelopeError(Envelope,
				TEXT("NOT_FOUND"),
				FString::Printf(TEXT("Class '%s' not found"), *ClassName));
			Envelope->SetStringField(TEXT("class_name"), TEXT(""));
			Envelope->SetArrayField(TEXT("properties"), {});
			Envelope->SetArrayField(TEXT("functions"), {});
			return Envelope;
		}

		// --- Build successful envelope ---
		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L1.reflection.class_schema") },
			{},
			true);

		// class_name: normalized to full path
		Envelope->SetStringField(TEXT("class_name"), Class->GetPathName());

		// --- Properties ---
		TArray<TSharedPtr<FJsonValue>> PropertiesArray;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			FProperty* Prop = *It;

			// Filter by inheritance
			if (!bIncludeInherited && Prop->GetOwnerClass() != Class)
			{
				continue;
			}

			TSharedPtr<FJsonObject> PropDesc = MCPReflection::BuildPropertyDesc(Prop, bIncludeMetadata);
			if (PropDesc.IsValid())
			{
				PropertiesArray.Add(MakeShared<FJsonValueObject>(PropDesc));
			}
		}
		Envelope->SetArrayField(TEXT("properties"), PropertiesArray);

		// --- Functions ---
		TArray<TSharedPtr<FJsonValue>> FunctionsArray;
		if (bIncludeFunctions)
		{
			for (TFieldIterator<UFunction> It(Class); It; ++It)
			{
				UFunction* Func = *It;

				// Filter by inheritance
				if (!bIncludeInherited && Func->GetOwnerClass() != Class)
				{
					continue;
				}

				// Only include BlueprintCallable functions
				if (!(Func->FunctionFlags & FUNC_BlueprintCallable))
				{
					continue;
				}

				TSharedPtr<FJsonObject> FuncDesc = MCPReflection::BuildFunctionDesc(Func);
				if (FuncDesc.IsValid())
				{
					FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncDesc));
				}
			}
		}
		Envelope->SetArrayField(TEXT("functions"), FunctionsArray);

		return Envelope;
	}

	/**
	 * Register ue_class_schema tool into the given registry.
	 */
	inline void Register(FMCPToolRegistry& Registry)
	{
		TSharedPtr<FJsonObject> InputSchema = Registry.ExtractInputSchema(SchemaDefName);
		if (!InputSchema.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("ue_class_schema: failed to extract inputSchema, tool will not be registered"));
			return;
		}

		FMCPToolRegistration Reg;
		Reg.Descriptor.Name = ToolName;
		Reg.Descriptor.Description = ToolDescription;
		Reg.Descriptor.InputSchema = InputSchema;

		Reg.Execute = [](const TSharedPtr<FJsonObject>& Arguments) -> TSharedPtr<FJsonObject>
		{
			// If already on GameThread, execute directly
			if (IsInGameThread())
			{
				return ExecuteOnGameThread(Arguments);
			}

			// Dispatch to GameThread and wait
			TSharedPtr<FJsonObject> Envelope;
			FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

			AsyncTask(ENamedThreads::GameThread, [&Envelope, &Arguments, DoneEvent]()
			{
				Envelope = ExecuteOnGameThread(Arguments);
				DoneEvent->Trigger();
			});

			DoneEvent->Wait();
			FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

			// Safety fallback — should never happen
			if (!Envelope.IsValid())
			{
				Envelope = MCPEnvelope::MakeEnvelope({}, {}, true);
				MCPEnvelope::SetEnvelopeError(Envelope,
					TEXT("INTERNAL"),
					TEXT("Reflection error: envelope was null after GameThread execution"));
				Envelope->SetStringField(TEXT("class_name"), TEXT(""));
				Envelope->SetArrayField(TEXT("properties"), {});
				Envelope->SetArrayField(TEXT("functions"), {});
			}

			return Envelope;
		};

		Registry.RegisterTool(MoveTemp(Reg));
	}
}
