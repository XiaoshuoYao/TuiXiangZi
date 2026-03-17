// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;

/**
 * Asset editing tools: create, duplicate, rename, move, save, delete, fix redirectors,
 * get creation schema, undo, and redo.
 *
 * All write operations require GameThread execution and are wrapped in FScopedTransaction.
 */
namespace MCPTool_AssetEdit
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
