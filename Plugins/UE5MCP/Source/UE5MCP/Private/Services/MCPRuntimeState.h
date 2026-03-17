// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"

/**
 * Immutable snapshot of runtime state — taken under lock, safe to read on any thread.
 */
struct FMCPRuntimeStateSnapshot
{
	FGuid SessionId;
	bool bReadOnly = false;
	int32 MaxResultBytes = 65536;
	int32 MaxItems = 500;
	FString BindHost = TEXT("127.0.0.1");
	uint32 Port = 0;
};

/**
 * Thread-safe runtime configuration store.
 * Created once in StartupModule, destroyed in ShutdownModule.
 */
struct FMCPRuntimeState
{
	FMCPRuntimeState()
	{
		SessionId = FGuid::NewGuid();
	}

	/** Take a consistent snapshot of all fields (under lock). */
	FMCPRuntimeStateSnapshot GetSnapshot() const
	{
		FScopeLock Lock(&Mutex);
		FMCPRuntimeStateSnapshot S;
		S.SessionId = SessionId;
		S.bReadOnly = bReadOnly;
		S.MaxResultBytes = MaxResultBytes;
		S.MaxItems = MaxItems;
		S.BindHost = BindHost;
		S.Port = Port;
		return S;
	}

	/** Apply a settings patch. Returns error message if validation fails, empty on success. */
	FString ApplyPatch(const TSharedPtr<FJsonObject>& Patch)
	{
		if (!Patch.IsValid()) return TEXT("");

		FScopeLock Lock(&Mutex);

		// read_only
		bool bNewReadOnly;
		if (Patch->TryGetBoolField(TEXT("read_only"), bNewReadOnly))
		{
			bReadOnly = bNewReadOnly;
		}

		// max_result_bytes
		int32 NewMaxResultBytes;
		if (Patch->TryGetNumberField(TEXT("max_result_bytes"), NewMaxResultBytes))
		{
			if (NewMaxResultBytes < 1024)
			{
				return TEXT("max_result_bytes must be >= 1024");
			}
			MaxResultBytes = NewMaxResultBytes;
		}

		// max_items
		int32 NewMaxItems;
		if (Patch->TryGetNumberField(TEXT("max_items"), NewMaxItems))
		{
			if (NewMaxItems < 1)
			{
				return TEXT("max_items must be >= 1");
			}
			MaxItems = NewMaxItems;
		}

		return TEXT("");
	}

	/** Set the actual bound port (called once during server startup). */
	void SetPort(uint32 InPort)
	{
		FScopeLock Lock(&Mutex);
		Port = InPort;
	}

private:
	mutable FCriticalSection Mutex;

	FGuid SessionId;
	bool bReadOnly = false;
	int32 MaxResultBytes = 65536;
	int32 MaxItems = 500;
	FString BindHost = TEXT("127.0.0.1");
	uint32 Port = 0;
};
