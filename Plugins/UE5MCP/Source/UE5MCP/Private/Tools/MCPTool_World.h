// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MCPToolRegistry.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;

/**
 * Reflection/Object + World tools:
 *   ue_object_inspect, ue_world_list, ue_actor_search, ue_selection_get
 *
 * All require GameThread. Implementation in MCPTool_World.cpp
 */
namespace MCPTool_World
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
