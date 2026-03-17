// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MCPToolRegistry.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;

/**
 * Asset tools: ue_asset_search, ue_asset_get, ue_asset_dependencies,
 * ue_asset_referencers, ue_asset_tags_index
 *
 * All require GameThread execution (AssetRegistry access).
 * Declaration only — implementation in MCPTool_Assets.cpp
 */
namespace MCPTool_Assets
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
