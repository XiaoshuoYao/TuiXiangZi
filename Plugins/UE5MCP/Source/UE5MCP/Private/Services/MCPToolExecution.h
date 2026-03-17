// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Unified GameThread execution helpers and parameter extraction utilities.
 * Extracted from MCPTool_ClassSchema's dispatch pattern for reuse across all tools.
 */
namespace MCPToolExecution
{
	// ---- Error code constants ----
	static const FString ErrorInvalidArgument = TEXT("INVALID_ARGUMENT");
	static const FString ErrorNotFound         = TEXT("NOT_FOUND");
	static const FString ErrorInternal         = TEXT("INTERNAL");
	static const FString ErrorReadOnly         = TEXT("READ_ONLY");
	static const FString ErrorUnsupported      = TEXT("UNSUPPORTED_CONTEXT");

	/**
	 * Execute a lambda on the GameThread and return its result.
	 * - If already on GameThread, executes directly.
	 * - Otherwise, dispatches via AsyncTask + FEvent wait.
	 */
	TSharedPtr<FJsonObject> RunOnGameThread(TFunction<TSharedPtr<FJsonObject>()> Work);

	// ---- Safe parameter extraction with defaults ----
	FString GetStringParam(const TSharedPtr<FJsonObject>& Args, const FString& Key, const FString& Default = TEXT(""));
	int32 GetIntParam(const TSharedPtr<FJsonObject>& Args, const FString& Key, int32 Default = 0);
	bool GetBoolParam(const TSharedPtr<FJsonObject>& Args, const FString& Key, bool Default = false);
	TArray<FString> GetStringArrayParam(const TSharedPtr<FJsonObject>& Args, const FString& Key);
}
