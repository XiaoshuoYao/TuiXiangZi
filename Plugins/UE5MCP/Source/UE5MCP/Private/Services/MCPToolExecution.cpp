// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/MCPToolExecution.h"
#include "MCPToolRegistry.h"

#include "Async/Async.h"
#include "HAL/Event.h"

TSharedPtr<FJsonObject> MCPToolExecution::RunOnGameThread(TFunction<TSharedPtr<FJsonObject>()> Work)
{
	if (IsInGameThread())
	{
		return Work();
	}

	TSharedPtr<FJsonObject> Result;
	FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

	AsyncTask(ENamedThreads::GameThread, [&Result, &Work, DoneEvent]()
	{
		Result = Work();
		DoneEvent->Trigger();
	});

	DoneEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(DoneEvent);

	if (!Result.IsValid())
	{
		Result = MCPEnvelope::MakeEnvelope({}, {}, true);
		MCPEnvelope::SetEnvelopeError(Result, ErrorInternal, TEXT("GameThread execution returned null"));
	}

	return Result;
}

FString MCPToolExecution::GetStringParam(const TSharedPtr<FJsonObject>& Args, const FString& Key, const FString& Default)
{
	if (!Args.IsValid()) return Default;
	FString Value;
	if (Args->TryGetStringField(Key, Value))
	{
		return Value;
	}
	return Default;
}

int32 MCPToolExecution::GetIntParam(const TSharedPtr<FJsonObject>& Args, const FString& Key, int32 Default)
{
	if (!Args.IsValid()) return Default;
	int32 Value;
	if (Args->TryGetNumberField(Key, Value))
	{
		return Value;
	}
	return Default;
}

bool MCPToolExecution::GetBoolParam(const TSharedPtr<FJsonObject>& Args, const FString& Key, bool Default)
{
	if (!Args.IsValid()) return Default;
	bool Value;
	if (Args->TryGetBoolField(Key, Value))
	{
		return Value;
	}
	return Default;
}

TArray<FString> MCPToolExecution::GetStringArrayParam(const TSharedPtr<FJsonObject>& Args, const FString& Key)
{
	TArray<FString> Result;
	if (!Args.IsValid()) return Result;

	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
	if (Args->TryGetArrayField(Key, ArrayPtr) && ArrayPtr)
	{
		for (const auto& Val : *ArrayPtr)
		{
			FString Str;
			if (Val->TryGetString(Str))
			{
				Result.Add(Str);
			}
		}
	}
	return Result;
}
