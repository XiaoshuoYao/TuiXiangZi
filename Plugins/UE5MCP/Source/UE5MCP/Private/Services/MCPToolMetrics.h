// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/CriticalSection.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMCPMetrics, Log, All);

/**
 * Per-tool aggregated statistics.
 */
struct FToolStats
{
	int32 TotalCalls = 0;
	int32 SuccessCount = 0;
	int32 ErrorCount = 0;
	double TotalDurationMs = 0.0;
	double MinDurationMs = TNumericLimits<double>::Max();
	double MaxDurationMs = 0.0;
	FDateTime FirstCalledAt;
	FDateTime LastCalledAt;
	FString LastError;
	TMap<FString, int32> ErrorCodes;
	TMap<FString, int32> CallsPerSession;

	TSharedPtr<FJsonObject> ToJson() const
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("total_calls"), TotalCalls);
		Obj->SetNumberField(TEXT("success_count"), SuccessCount);
		Obj->SetNumberField(TEXT("error_count"), ErrorCount);
		Obj->SetNumberField(TEXT("total_duration_ms"), TotalDurationMs);
		Obj->SetNumberField(TEXT("avg_duration_ms"), TotalCalls > 0 ? TotalDurationMs / TotalCalls : 0.0);
		Obj->SetNumberField(TEXT("min_duration_ms"), MinDurationMs == TNumericLimits<double>::Max() ? 0.0 : MinDurationMs);
		Obj->SetNumberField(TEXT("max_duration_ms"), MaxDurationMs);
		Obj->SetStringField(TEXT("first_called_at"), FirstCalledAt.ToIso8601());
		Obj->SetStringField(TEXT("last_called_at"), LastCalledAt.ToIso8601());
		Obj->SetStringField(TEXT("last_error"), LastError);

		TSharedPtr<FJsonObject> ErrorCodesObj = MakeShared<FJsonObject>();
		for (const auto& Pair : ErrorCodes)
		{
			ErrorCodesObj->SetNumberField(Pair.Key, Pair.Value);
		}
		Obj->SetObjectField(TEXT("error_codes"), ErrorCodesObj);

		TSharedPtr<FJsonObject> SessionObj = MakeShared<FJsonObject>();
		for (const auto& Pair : CallsPerSession)
		{
			SessionObj->SetNumberField(Pair.Key, Pair.Value);
		}
		Obj->SetObjectField(TEXT("calls_per_session"), SessionObj);

		return Obj;
	}

	static FToolStats FromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FToolStats S;
		if (!Obj.IsValid()) return S;

		Obj->TryGetNumberField(TEXT("total_calls"), S.TotalCalls);
		Obj->TryGetNumberField(TEXT("success_count"), S.SuccessCount);
		Obj->TryGetNumberField(TEXT("error_count"), S.ErrorCount);
		Obj->TryGetNumberField(TEXT("total_duration_ms"), S.TotalDurationMs);
		// avg is computed, skip loading

		double MinD = 0;
		if (Obj->TryGetNumberField(TEXT("min_duration_ms"), MinD))
		{
			S.MinDurationMs = MinD;
		}
		Obj->TryGetNumberField(TEXT("max_duration_ms"), S.MaxDurationMs);

		FString FirstStr, LastStr;
		if (Obj->TryGetStringField(TEXT("first_called_at"), FirstStr))
		{
			FDateTime::ParseIso8601(*FirstStr, S.FirstCalledAt);
		}
		if (Obj->TryGetStringField(TEXT("last_called_at"), LastStr))
		{
			FDateTime::ParseIso8601(*LastStr, S.LastCalledAt);
		}

		Obj->TryGetStringField(TEXT("last_error"), S.LastError);

		const TSharedPtr<FJsonObject>* ErrorCodesPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("error_codes"), ErrorCodesPtr) && ErrorCodesPtr)
		{
			for (const auto& Pair : (*ErrorCodesPtr)->Values)
			{
				int32 Count = 0;
				if (Pair.Value->TryGetNumber(Count))
				{
					S.ErrorCodes.Add(Pair.Key, Count);
				}
			}
		}

		const TSharedPtr<FJsonObject>* SessionPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("calls_per_session"), SessionPtr) && SessionPtr)
		{
			for (const auto& Pair : (*SessionPtr)->Values)
			{
				int32 Count = 0;
				if (Pair.Value->TryGetNumber(Count))
				{
					S.CallsPerSession.Add(Pair.Key, Count);
				}
			}
		}

		return S;
	}
};

/**
 * Error log entry for the ring buffer.
 */
struct FToolErrorEntry
{
	FString ToolName;
	FString SessionId;
	FDateTime Timestamp;
	double DurationMs = 0.0;
	FString ErrorCode;
	FString ErrorMessage;
	TSharedPtr<FJsonObject> Arguments;

	TSharedPtr<FJsonObject> ToJson() const
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("tool_name"), ToolName);
		Obj->SetStringField(TEXT("session_id"), SessionId);
		Obj->SetStringField(TEXT("timestamp"), Timestamp.ToIso8601());
		Obj->SetNumberField(TEXT("duration_ms"), DurationMs);
		Obj->SetStringField(TEXT("error_code"), ErrorCode);
		Obj->SetStringField(TEXT("error_message"), ErrorMessage);
		if (Arguments.IsValid())
		{
			Obj->SetObjectField(TEXT("arguments"), Arguments);
		}
		return Obj;
	}

	static FToolErrorEntry FromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FToolErrorEntry E;
		if (!Obj.IsValid()) return E;

		Obj->TryGetStringField(TEXT("tool_name"), E.ToolName);
		Obj->TryGetStringField(TEXT("session_id"), E.SessionId);

		FString TsStr;
		if (Obj->TryGetStringField(TEXT("timestamp"), TsStr))
		{
			FDateTime::ParseIso8601(*TsStr, E.Timestamp);
		}

		Obj->TryGetNumberField(TEXT("duration_ms"), E.DurationMs);
		Obj->TryGetStringField(TEXT("error_code"), E.ErrorCode);
		Obj->TryGetStringField(TEXT("error_message"), E.ErrorMessage);

		const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("arguments"), ArgsPtr) && ArgsPtr)
		{
			E.Arguments = *ArgsPtr;
		}

		return E;
	}
};

/**
 * Tool Metrics Service — collects usage statistics and error logs for all MCP tool calls.
 * Persists data to {Saved}/UEMCP/tool_metrics.json and tool_errors.json.
 * Thread-safe. Flushes to disk periodically and on shutdown.
 */
class FMCPToolMetrics
{
public:
	FMCPToolMetrics();

	/** Record a tool call result. Called after every tool execution. */
	void RecordCall(
		const FString& ToolName,
		const FString& SessionId,
		const TSharedPtr<FJsonObject>& Arguments,
		const TSharedPtr<FJsonObject>& Envelope,
		double DurationMs);

	/** Load historical data from disk. */
	void LoadFromDisk();

	/** Save current data to disk (async). */
	void SaveToDisk();

	/** Synchronous save + cleanup. Called on module shutdown. */
	void Shutdown();

private:
	/** Classify an error message into a standardized error code. */
	static FString ClassifyError(const FString& ErrorMessage);

	/** Append an error entry to the ring buffer. */
	void AppendError(const FToolErrorEntry& Entry);

	/** Schedule a deferred flush if not already scheduled. */
	void ScheduleFlush();

	/** Actual disk write (both files). */
	void FlushToDisk();

	/** Build the metrics JSON for writing. */
	TSharedPtr<FJsonObject> BuildMetricsJson() const;

	/** Build the errors JSON for writing. */
	TSharedPtr<FJsonObject> BuildErrorsJson() const;

	// ---- State ----
	TMap<FString, FToolStats> ToolStatsMap;
	TArray<FToolErrorEntry> ErrorLog;
	mutable FCriticalSection Mutex;
	int32 PendingCallCount = 0;
	int32 SessionCount = 0;
	int32 TotalCalls = 0;
	bool bDirty = false;
	bool bShutdown = false;
	FTimerHandle FlushTimerHandle;
	FString StoragePath;

	static constexpr int32 MaxErrorEntries = 500;
	static constexpr int32 FlushCallThreshold = 10;
	static constexpr float FlushIntervalSeconds = 60.0f;
};
