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
 * Material tools (Editor-only):
 *   ue_mat_params_get     - List material/material instance parameters
 *   ue_mat_graph_get      - Export material expression graph as JSON
 *   ue_mat_find_param_usage - Find parameter usage across project
 *
 * Implementation in MCPTool_Materials.cpp
 */
namespace MCPTool_Materials
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
