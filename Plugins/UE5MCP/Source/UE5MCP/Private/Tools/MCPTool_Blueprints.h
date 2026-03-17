// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MCPToolRegistry.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;

/**
 * Blueprint tools (Editor-only):
 *   ue_bp_list, ue_bp_graph_list, ue_bp_graph_get,
 *   ue_bp_function_summary, ue_bp_find_calls, ue_bp_find_property_access
 *
 * Implementation in MCPTool_Blueprints.cpp
 */
namespace MCPTool_Blueprints
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
