// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/OutputDevice.h"
#include "Misc/DateTime.h"

/**
 * Ring buffer log capture device.
 * Registers as a GLog output device to capture UE log messages.
 * Thread-safe for concurrent writes (from any logging thread) and reads (from HTTP worker).
 */
class FMCPLogBuffer : public FOutputDevice
{
public:
	static constexpr int32 MaxCapacity = 5000;

	FMCPLogBuffer()
	{
		RingBuffer.SetNum(MaxCapacity);
	}

	virtual ~FMCPLogBuffer() override = default;

	// FOutputDevice interface
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		FScopeLock Lock(&Mutex);

		FMCPLogEntry& Entry = RingBuffer[WriteIndex % MaxCapacity];
		Entry.Category = Category;
		Entry.Verbosity = Verbosity;
		Entry.Message = V;
		Entry.Timestamp = FDateTime::UtcNow();

		WriteIndex++;
		if (Count < MaxCapacity)
		{
			Count++;
		}
	}

	/**
	 * Get the last N log entries, optionally filtered by category names.
	 * @param NumLines Number of lines to retrieve (clamped to available)
	 * @param Channels If non-empty, only return entries matching these category names (exact match)
	 * @return Array of formatted log lines: "[Category] Message"
	 */
	TArray<FString> GetTail(int32 NumLines, const TArray<FString>& Channels = {}) const
	{
		FScopeLock Lock(&Mutex);

		TArray<FString> Result;
		Result.Reserve(FMath::Min(NumLines, Count));

		// Convert channel filter to FName set for fast lookup
		TSet<FName> ChannelFilter;
		for (const FString& Ch : Channels)
		{
			ChannelFilter.Add(FName(*Ch));
		}

		// Walk backwards from most recent
		int32 StartIdx = WriteIndex - 1;
		for (int32 i = 0; i < Count && Result.Num() < NumLines; ++i)
		{
			int32 Idx = ((StartIdx - i) % MaxCapacity + MaxCapacity) % MaxCapacity;
			const FMCPLogEntry& Entry = RingBuffer[Idx];

			// Channel filter
			if (ChannelFilter.Num() > 0 && !ChannelFilter.Contains(Entry.Category))
			{
				continue;
			}

			Result.Add(FString::Printf(TEXT("[%s] %s"), *Entry.Category.ToString(), *Entry.Message));
		}

		// Reverse so oldest is first
		Algo::Reverse(Result);
		return Result;
	}

private:
	struct FMCPLogEntry
	{
		FName Category;
		ELogVerbosity::Type Verbosity = ELogVerbosity::Log;
		FString Message;
		FDateTime Timestamp;
	};

	mutable FCriticalSection Mutex;
	TArray<FMCPLogEntry> RingBuffer;
	int32 WriteIndex = 0;
	int32 Count = 0;
};
