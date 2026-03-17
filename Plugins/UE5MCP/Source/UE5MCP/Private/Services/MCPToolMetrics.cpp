// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/MCPToolMetrics.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogMCPMetrics);

FMCPToolMetrics::FMCPToolMetrics()
{
	StoragePath = FPaths::ProjectSavedDir() / TEXT("UEMCP");
}

// ============================================================================
// RecordCall
// ============================================================================

void FMCPToolMetrics::RecordCall(
	const FString& ToolName,
	const FString& SessionId,
	const TSharedPtr<FJsonObject>& Arguments,
	const TSharedPtr<FJsonObject>& Envelope,
	double DurationMs)
{
	FScopeLock Lock(&Mutex);

	if (bShutdown) return;

	// Determine if error
	bool bIsError = Envelope.IsValid() && Envelope->HasField(TEXT("error"));
	FString ErrorMessage;
	if (bIsError)
	{
		Envelope->TryGetStringField(TEXT("error"), ErrorMessage);
		// If error is an object, try to get the message field
		if (ErrorMessage.IsEmpty())
		{
			const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
			if (Envelope->TryGetObjectField(TEXT("error"), ErrorObj) && ErrorObj)
			{
				(*ErrorObj)->TryGetStringField(TEXT("message"), ErrorMessage);
			}
		}
	}

	// Update or create tool stats
	FToolStats& Stats = ToolStatsMap.FindOrAdd(ToolName);
	Stats.TotalCalls++;
	if (bIsError)
	{
		Stats.ErrorCount++;
	}
	else
	{
		Stats.SuccessCount++;
	}

	Stats.TotalDurationMs += DurationMs;
	if (DurationMs < Stats.MinDurationMs)
	{
		Stats.MinDurationMs = DurationMs;
	}
	if (DurationMs > Stats.MaxDurationMs)
	{
		Stats.MaxDurationMs = DurationMs;
	}

	FDateTime Now = FDateTime::UtcNow();
	if (Stats.TotalCalls == 1)
	{
		Stats.FirstCalledAt = Now;
	}
	Stats.LastCalledAt = Now;

	// Session tracking
	int32& SessionCallCount = Stats.CallsPerSession.FindOrAdd(SessionId);
	SessionCallCount++;

	// Error details
	if (bIsError)
	{
		FString ErrorCode = ClassifyError(ErrorMessage);
		Stats.LastError = ErrorMessage;
		int32& CodeCount = Stats.ErrorCodes.FindOrAdd(ErrorCode);
		CodeCount++;

		// Append to error log ring buffer
		FToolErrorEntry Entry;
		Entry.ToolName = ToolName;
		Entry.SessionId = SessionId;
		Entry.Timestamp = Now;
		Entry.DurationMs = DurationMs;
		Entry.ErrorCode = ErrorCode;
		Entry.ErrorMessage = ErrorMessage;
		Entry.Arguments = Arguments;
		AppendError(Entry);
	}

	TotalCalls++;
	PendingCallCount++;
	bDirty = true;

	// Check if we should flush
	if (PendingCallCount >= FlushCallThreshold)
	{
		PendingCallCount = 0;
		// Flush async
		TSharedPtr<FJsonObject> MetricsJson = BuildMetricsJson();
		TSharedPtr<FJsonObject> ErrorsJson = BuildErrorsJson();
		FString Path = StoragePath;

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [MetricsJson, ErrorsJson, Path]()
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			PlatformFile.CreateDirectoryTree(*Path);

			// Write metrics
			{
				FString Str;
				auto Writer = TJsonWriterFactory<>::Create(&Str);
				FJsonSerializer::Serialize(MetricsJson.ToSharedRef(), Writer);
				FFileHelper::SaveStringToFile(Str, *(Path / TEXT("tool_metrics.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}

			// Write errors
			{
				FString Str;
				auto Writer = TJsonWriterFactory<>::Create(&Str);
				FJsonSerializer::Serialize(ErrorsJson.ToSharedRef(), Writer);
				FFileHelper::SaveStringToFile(Str, *(Path / TEXT("tool_errors.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}

			UE_LOG(LogMCPMetrics, Verbose, TEXT("Tool metrics flushed to disk (call-count trigger)"));
		});
	}
}

// ============================================================================
// Error Classification
// ============================================================================

FString FMCPToolMetrics::ClassifyError(const FString& ErrorMessage)
{
	FString Msg = ErrorMessage.ToLower();

	// Priority order matching
	if (Msg.Contains(TEXT("read-only")) || Msg.Contains(TEXT("read only")))
	{
		return TEXT("READ_ONLY");
	}
	if (Msg.Contains(TEXT("timeout")))
	{
		return TEXT("GAME_THREAD_TIMEOUT");
	}
	if (Msg.Contains(TEXT("not found")) && Msg.Contains(TEXT("asset")))
	{
		return TEXT("ASSET_NOT_FOUND");
	}
	if (Msg.Contains(TEXT("not found")) && Msg.Contains(TEXT("actor")))
	{
		return TEXT("ACTOR_NOT_FOUND");
	}
	if (Msg.Contains(TEXT("not found")) && Msg.Contains(TEXT("class")))
	{
		return TEXT("CLASS_NOT_FOUND");
	}
	if (Msg.Contains(TEXT("not found")) && Msg.Contains(TEXT("property")))
	{
		return TEXT("PROPERTY_NOT_FOUND");
	}
	if (Msg.Contains(TEXT("param")) || Msg.Contains(TEXT("missing")) || Msg.Contains(TEXT("invalid")))
	{
		return TEXT("INVALID_PARAMS");
	}
	if (Msg.Contains(TEXT("compile")))
	{
		return TEXT("BLUEPRINT_COMPILE_ERROR");
	}
	if (Msg.Contains(TEXT("serialize")) || Msg.Contains(TEXT("json")) || Msg.Contains(TEXT("parse")))
	{
		return TEXT("SERIALIZATION_ERROR");
	}
	if (Msg.Contains(TEXT("locked")) || Msg.Contains(TEXT("permission")))
	{
		return TEXT("PERMISSION_DENIED");
	}
	if (Msg.Contains(TEXT("truncat")) || Msg.Contains(TEXT("exceed")) || Msg.Contains(TEXT("limit")))
	{
		return TEXT("RESOURCE_LIMIT");
	}

	return TEXT("INTERNAL_ERROR");
}

// ============================================================================
// Ring Buffer
// ============================================================================

void FMCPToolMetrics::AppendError(const FToolErrorEntry& Entry)
{
	// Caller already holds Mutex
	if (ErrorLog.Num() >= MaxErrorEntries)
	{
		ErrorLog.RemoveAt(0);
	}
	ErrorLog.Add(Entry);
}

// ============================================================================
// Flush / Timer
// ============================================================================

void FMCPToolMetrics::ScheduleFlush()
{
	if (!GEngine) return;

	UWorld* World = GEngine->GetCurrentPlayWorld();
	if (!World) return;

	World->GetTimerManager().SetTimer(
		FlushTimerHandle,
		FTimerDelegate::CreateLambda([this]()
		{
			FScopeLock Lock(&Mutex);
			if (bDirty && !bShutdown)
			{
				FlushToDisk();
			}
		}),
		FlushIntervalSeconds,
		true // looping
	);
}

void FMCPToolMetrics::FlushToDisk()
{
	// Caller holds Mutex
	TSharedPtr<FJsonObject> MetricsJson = BuildMetricsJson();
	TSharedPtr<FJsonObject> ErrorsJson = BuildErrorsJson();
	FString Path = StoragePath;
	bDirty = false;
	PendingCallCount = 0;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [MetricsJson, ErrorsJson, Path]()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*Path);

		{
			FString Str;
			auto Writer = TJsonWriterFactory<>::Create(&Str);
			FJsonSerializer::Serialize(MetricsJson.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(Str, *(Path / TEXT("tool_metrics.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		{
			FString Str;
			auto Writer = TJsonWriterFactory<>::Create(&Str);
			FJsonSerializer::Serialize(ErrorsJson.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(Str, *(Path / TEXT("tool_errors.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		UE_LOG(LogMCPMetrics, Verbose, TEXT("Tool metrics flushed to disk (timer trigger)"));
	});
}

// ============================================================================
// Build JSON
// ============================================================================

TSharedPtr<FJsonObject> FMCPToolMetrics::BuildMetricsJson() const
{
	// Caller holds Mutex
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetStringField(TEXT("last_updated"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("session_count"), SessionCount);
	Root->SetNumberField(TEXT("total_calls"), TotalCalls);

	TSharedPtr<FJsonObject> ToolsObj = MakeShared<FJsonObject>();
	for (const auto& Pair : ToolStatsMap)
	{
		ToolsObj->SetObjectField(Pair.Key, Pair.Value.ToJson());
	}
	Root->SetObjectField(TEXT("tools"), ToolsObj);

	return Root;
}

TSharedPtr<FJsonObject> FMCPToolMetrics::BuildErrorsJson() const
{
	// Caller holds Mutex
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema_version"), 1);
	Root->SetNumberField(TEXT("max_entries"), MaxErrorEntries);

	TArray<TSharedPtr<FJsonValue>> ErrorsArr;
	for (const FToolErrorEntry& Entry : ErrorLog)
	{
		ErrorsArr.Add(MakeShared<FJsonValueObject>(Entry.ToJson()));
	}
	Root->SetArrayField(TEXT("errors"), ErrorsArr);

	return Root;
}

// ============================================================================
// Persistence
// ============================================================================

void FMCPToolMetrics::LoadFromDisk()
{
	FScopeLock Lock(&Mutex);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*StoragePath);

	// Load metrics
	FString MetricsPath = StoragePath / TEXT("tool_metrics.json");
	FString MetricsStr;
	if (FFileHelper::LoadFileToString(MetricsStr, *MetricsPath))
	{
		TSharedPtr<FJsonObject> MetricsJson;
		auto Reader = TJsonReaderFactory<>::Create(MetricsStr);
		if (FJsonSerializer::Deserialize(Reader, MetricsJson) && MetricsJson.IsValid())
		{
			double SC = 0;
			if (MetricsJson->TryGetNumberField(TEXT("session_count"), SC))
			{
				SessionCount = static_cast<int32>(SC) + 1; // increment for this new session
			}

			double TC = 0;
			if (MetricsJson->TryGetNumberField(TEXT("total_calls"), TC))
			{
				TotalCalls = static_cast<int32>(TC);
			}

			const TSharedPtr<FJsonObject>* ToolsPtr = nullptr;
			if (MetricsJson->TryGetObjectField(TEXT("tools"), ToolsPtr) && ToolsPtr)
			{
				for (const auto& Pair : (*ToolsPtr)->Values)
				{
					const TSharedPtr<FJsonObject>* ToolObj = nullptr;
					if (Pair.Value->TryGetObject(ToolObj) && ToolObj)
					{
						ToolStatsMap.Add(Pair.Key, FToolStats::FromJson(*ToolObj));
					}
				}
			}

			UE_LOG(LogMCPMetrics, Log, TEXT("Loaded tool metrics: %d tools, %d total calls, session #%d"),
				ToolStatsMap.Num(), TotalCalls, SessionCount);
		}
	}
	else
	{
		SessionCount = 1;
		UE_LOG(LogMCPMetrics, Log, TEXT("No existing tool metrics found, starting fresh (session #1)"));
	}

	// Load errors
	FString ErrorsPath = StoragePath / TEXT("tool_errors.json");
	FString ErrorsStr;
	if (FFileHelper::LoadFileToString(ErrorsStr, *ErrorsPath))
	{
		TSharedPtr<FJsonObject> ErrorsJson;
		auto Reader = TJsonReaderFactory<>::Create(ErrorsStr);
		if (FJsonSerializer::Deserialize(Reader, ErrorsJson) && ErrorsJson.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* ErrorsArr = nullptr;
			if (ErrorsJson->TryGetArrayField(TEXT("errors"), ErrorsArr))
			{
				for (const auto& Val : *ErrorsArr)
				{
					const TSharedPtr<FJsonObject>* EntryObj = nullptr;
					if (Val->TryGetObject(EntryObj) && EntryObj)
					{
						ErrorLog.Add(FToolErrorEntry::FromJson(*EntryObj));
					}
				}
			}

			UE_LOG(LogMCPMetrics, Log, TEXT("Loaded %d error log entries"), ErrorLog.Num());
		}
	}
}

void FMCPToolMetrics::SaveToDisk()
{
	FScopeLock Lock(&Mutex);
	FlushToDisk();
}

void FMCPToolMetrics::Shutdown()
{
	FScopeLock Lock(&Mutex);
	bShutdown = true;

	if (!bDirty) return;

	// Synchronous write on shutdown
	TSharedPtr<FJsonObject> MetricsJson = BuildMetricsJson();
	TSharedPtr<FJsonObject> ErrorsJson = BuildErrorsJson();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*StoragePath);

	{
		FString Str;
		auto Writer = TJsonWriterFactory<>::Create(&Str);
		FJsonSerializer::Serialize(MetricsJson.ToSharedRef(), Writer);
		FFileHelper::SaveStringToFile(Str, *(StoragePath / TEXT("tool_metrics.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	{
		FString Str;
		auto Writer = TJsonWriterFactory<>::Create(&Str);
		FJsonSerializer::Serialize(ErrorsJson.ToSharedRef(), Writer);
		FFileHelper::SaveStringToFile(Str, *(StoragePath / TEXT("tool_errors.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bDirty = false;
	UE_LOG(LogMCPMetrics, Log, TEXT("Tool metrics saved on shutdown (%d tools, %d errors logged)"),
		ToolStatsMap.Num(), ErrorLog.Num());
}
