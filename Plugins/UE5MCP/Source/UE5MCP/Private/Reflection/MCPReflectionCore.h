// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Reflection utilities for MCP tools.
 * All functions must be called on the GameThread.
 */
namespace MCPReflection
{
	/** Resolve a class name to UClass* using multiple strategies. Returns nullptr on failure. */
	UClass* ResolveClass(const FString& ClassName);

	/** Map an FProperty to a human-readable UE type string. */
	FString MapPropertyType(FProperty* Property);

	/** Build a PropertyDesc JSON object from an FProperty. */
	TSharedPtr<FJsonObject> BuildPropertyDesc(FProperty* Property, bool bIncludeMetadata);

	/** Build a FunctionDesc JSON object from a UFunction. */
	TSharedPtr<FJsonObject> BuildFunctionDesc(UFunction* Function);
}
