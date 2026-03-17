// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MCPToolRegistry.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPLogBuffer.h"

#include "Misc/App.h"
#include "Misc/EngineVersion.h"

/**
 * Core tools: ue_ping, ue_capabilities, ue_settings_get, ue_settings_set, ue_log_tail
 */
namespace MCPTool_Core
{
	// ========================================================================
	// ue_ping
	// ========================================================================
	namespace Ping
	{
		static constexpr const TCHAR* ToolName = TEXT("ue_ping");
		static constexpr const TCHAR* ToolDescription =
			TEXT("Returns engine version, project name, and session ID. Use as a connectivity check.");
		static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_ping");

		inline TSharedPtr<FJsonObject> Execute(
			const TSharedPtr<FJsonObject>& /*Arguments*/,
			FMCPRuntimeState& RuntimeState)
		{
			FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
				{ TEXT("L0.core.ping") }, {}, true);

			Envelope->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
			Envelope->SetStringField(TEXT("project_name"), FApp::GetProjectName());
			Envelope->SetStringField(TEXT("session_id"), Snap.SessionId.ToString(EGuidFormats::DigitsWithHyphens));

			return Envelope;
		}
	}

	// ========================================================================
	// ue_capabilities
	// ========================================================================
	namespace Capabilities
	{
		static constexpr const TCHAR* ToolName = TEXT("ue_capabilities");
		static constexpr const TCHAR* ToolDescription =
			TEXT("Returns the list of capability providers and their current limits.");
		static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_capabilities");

		inline TSharedPtr<FJsonObject> Execute(
			const TSharedPtr<FJsonObject>& /*Arguments*/,
			FMCPRuntimeState& RuntimeState)
		{
			FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
				{ TEXT("L0.core.capabilities") }, {}, true);

			TSharedPtr<FJsonObject> Limits = MakeShared<FJsonObject>();
			Limits->SetNumberField(TEXT("max_result_bytes"), Snap.MaxResultBytes);
			Limits->SetNumberField(TEXT("max_items"), Snap.MaxItems);

			// Build provider list
			struct FProviderInfo { const TCHAR* Name; const TCHAR* Version; bool bEditorOnly; };
			TArray<FProviderInfo> Providers = {
				{ TEXT("core"),       TEXT("0.1.0"), false },
				{ TEXT("assets"),     TEXT("0.1.0"), false },
				{ TEXT("reflection"), TEXT("0.1.0"), false },
				{ TEXT("world"),      TEXT("0.1.0"), false },
#if WITH_EDITOR
				{ TEXT("blueprints"), TEXT("0.1.0"), true },
#endif
			};

			TArray<TSharedPtr<FJsonValue>> ProvidersArray;
			for (const auto& P : Providers)
			{
				TSharedPtr<FJsonObject> ProvObj = MakeShared<FJsonObject>();
				ProvObj->SetStringField(TEXT("name"), P.Name);
				ProvObj->SetStringField(TEXT("version"), P.Version);
				ProvObj->SetBoolField(TEXT("editor_only"), P.bEditorOnly);
				ProvObj->SetObjectField(TEXT("limits"), Limits);
				ProvidersArray.Add(MakeShared<FJsonValueObject>(ProvObj));
			}

			Envelope->SetArrayField(TEXT("providers"), ProvidersArray);
			return Envelope;
		}
	}

	// ========================================================================
	// ue_settings_get
	// ========================================================================
	namespace SettingsGet
	{
		static constexpr const TCHAR* ToolName = TEXT("ue_settings_get");
		static constexpr const TCHAR* ToolDescription =
			TEXT("Returns current runtime settings (read_only, max_result_bytes, max_items, bind_host, port).");
		static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_settings_get");

		inline TSharedPtr<FJsonObject> Execute(
			const TSharedPtr<FJsonObject>& /*Arguments*/,
			FMCPRuntimeState& RuntimeState)
		{
			FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
				{ TEXT("L0.core.settings") }, {}, true);

			TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
			Settings->SetBoolField(TEXT("read_only"), Snap.bReadOnly);
			Settings->SetNumberField(TEXT("max_result_bytes"), Snap.MaxResultBytes);
			Settings->SetNumberField(TEXT("max_items"), Snap.MaxItems);
			Settings->SetStringField(TEXT("bind_host"), Snap.BindHost);
			Settings->SetNumberField(TEXT("port"), static_cast<double>(Snap.Port));

			Envelope->SetObjectField(TEXT("settings"), Settings);
			return Envelope;
		}
	}

	// ========================================================================
	// ue_settings_set
	// ========================================================================
	namespace SettingsSet
	{
		static constexpr const TCHAR* ToolName = TEXT("ue_settings_set");
		static constexpr const TCHAR* ToolDescription =
			TEXT("Modify runtime settings. Accepts a patch object with read_only, max_result_bytes, max_items.");
		static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_settings_set");

		inline TSharedPtr<FJsonObject> Execute(
			const TSharedPtr<FJsonObject>& Arguments,
			FMCPRuntimeState& RuntimeState)
		{
			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
				{ TEXT("L0.core.settings") }, {}, true);

			// Extract patch object
			const TSharedPtr<FJsonObject>* PatchPtr = nullptr;
			TSharedPtr<FJsonObject> Patch;
			if (Arguments.IsValid() && Arguments->TryGetObjectField(TEXT("patch"), PatchPtr) && PatchPtr)
			{
				Patch = *PatchPtr;
			}

			if (!Patch.IsValid())
			{
				MCPEnvelope::SetEnvelopeError(Envelope,
					MCPToolExecution::ErrorInvalidArgument,
					TEXT("Missing required parameter: patch (object)"));
				return Envelope;
			}

			FString Error = RuntimeState.ApplyPatch(Patch);
			if (!Error.IsEmpty())
			{
				MCPEnvelope::SetEnvelopeError(Envelope,
					MCPToolExecution::ErrorInvalidArgument, Error);
			}

			return Envelope;
		}
	}

	// ========================================================================
	// ue_log_tail
	// ========================================================================
	namespace LogTail
	{
		static constexpr const TCHAR* ToolName = TEXT("ue_log_tail");
		static constexpr const TCHAR* ToolDescription =
			TEXT("Returns the last N lines from the UE log buffer, optionally filtered by log category.");
		static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_log_tail");

		inline TSharedPtr<FJsonObject> Execute(
			const TSharedPtr<FJsonObject>& Arguments,
			FMCPLogBuffer& LogBuffer)
		{
			int32 Lines = MCPToolExecution::GetIntParam(Arguments, TEXT("lines"), 200);
			TArray<FString> Channels = MCPToolExecution::GetStringArrayParam(Arguments, TEXT("channels"));

			TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
				{ TEXT("L0.core.log") }, {}, true);

			if (Lines < 1 || Lines > FMCPLogBuffer::MaxCapacity)
			{
				MCPEnvelope::SetEnvelopeError(Envelope,
					MCPToolExecution::ErrorInvalidArgument,
					FString::Printf(TEXT("lines must be between 1 and %d"), FMCPLogBuffer::MaxCapacity));
				Envelope->SetArrayField(TEXT("lines"), {});
				return Envelope;
			}

			TArray<FString> LogLines = LogBuffer.GetTail(Lines, Channels);

			TArray<TSharedPtr<FJsonValue>> LinesArray;
			LinesArray.Reserve(LogLines.Num());
			for (const FString& Line : LogLines)
			{
				LinesArray.Add(MakeShared<FJsonValueString>(Line));
			}
			Envelope->SetArrayField(TEXT("lines"), LinesArray);

			return Envelope;
		}
	}

	// ========================================================================
	// Registration
	// ========================================================================
	inline void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPLogBuffer& LogBuffer)
	{
		// ue_ping
		{
			TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(Ping::SchemaDefName);
			FMCPToolRegistration Reg;
			Reg.Descriptor.Name = Ping::ToolName;
			Reg.Descriptor.Description = Ping::ToolDescription;
			Reg.Descriptor.InputSchema = Schema;
			Reg.Execute = [&RuntimeState](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
			{
				return Ping::Execute(Args, RuntimeState);
			};
			Registry.RegisterTool(MoveTemp(Reg));
		}

		// ue_capabilities
		{
			TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(Capabilities::SchemaDefName);
			FMCPToolRegistration Reg;
			Reg.Descriptor.Name = Capabilities::ToolName;
			Reg.Descriptor.Description = Capabilities::ToolDescription;
			Reg.Descriptor.InputSchema = Schema;
			Reg.Execute = [&RuntimeState](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
			{
				return Capabilities::Execute(Args, RuntimeState);
			};
			Registry.RegisterTool(MoveTemp(Reg));
		}

		// ue_settings_get
		{
			TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(SettingsGet::SchemaDefName);
			FMCPToolRegistration Reg;
			Reg.Descriptor.Name = SettingsGet::ToolName;
			Reg.Descriptor.Description = SettingsGet::ToolDescription;
			Reg.Descriptor.InputSchema = Schema;
			Reg.Execute = [&RuntimeState](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
			{
				return SettingsGet::Execute(Args, RuntimeState);
			};
			Registry.RegisterTool(MoveTemp(Reg));
		}

		// ue_settings_set
		{
			TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(SettingsSet::SchemaDefName);
			FMCPToolRegistration Reg;
			Reg.Descriptor.Name = SettingsSet::ToolName;
			Reg.Descriptor.Description = SettingsSet::ToolDescription;
			Reg.Descriptor.InputSchema = Schema;
			Reg.Execute = [&RuntimeState](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
			{
				return SettingsSet::Execute(Args, RuntimeState);
			};
			Registry.RegisterTool(MoveTemp(Reg));
		}

		// ue_log_tail
		{
			TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(LogTail::SchemaDefName);
			FMCPToolRegistration Reg;
			Reg.Descriptor.Name = LogTail::ToolName;
			Reg.Descriptor.Description = LogTail::ToolDescription;
			Reg.Descriptor.InputSchema = Schema;
			Reg.Execute = [&LogBuffer](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
			{
				return LogTail::Execute(Args, LogBuffer);
			};
			Registry.RegisterTool(MoveTemp(Reg));
		}
	}
}
