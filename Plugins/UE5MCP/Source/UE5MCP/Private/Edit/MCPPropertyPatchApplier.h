// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Converts between JSON values and UE FProperty values.
 * Supports dot-path property resolution, reading properties as JSON,
 * and writing JSON values into properties.
 * All functions must be called on the GameThread.
 */
namespace MCPPropertyPatchApplier
{
	struct FPatchResult
	{
		bool bSuccess = false;
		FString ErrorCode;
		FString ErrorMessage;
		TSharedPtr<FJsonValue> OldValue;
		TSharedPtr<FJsonValue> NewValue;
	};

	struct FResolvedProperty
	{
		FProperty* Property = nullptr;
		void* ContainerPtr = nullptr;
		FString ErrorMessage;
		bool IsValid() const { return Property != nullptr && ContainerPtr != nullptr; }
	};

	/** Resolve a dot-separated property path (e.g. "SpawnConfig.Delay") on an object. */
	FResolvedProperty ResolvePropertyPath(UObject* Object, const FString& PropertyPath);

	/** Write a JSON value into the property at the given path. Returns old and new values. */
	FPatchResult ApplyPatch(UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& JsonValue);

	/** Read the current value of a property as a JSON value. */
	TSharedPtr<FJsonValue> ReadPropertyAsJson(FProperty* Property, const void* ContainerPtr);

	struct FBatchPatchResult
	{
		bool bAllSuccess = true;
		TArray<FPatchResult> Results;
	};

	/** Apply multiple patches to an object. Continues on failure. */
	FBatchPatchResult ApplyPatches(UObject* Object, const TArray<TPair<FString, TSharedPtr<FJsonValue>>>& Patches);
}
