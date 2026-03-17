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
 * Validation / Evaluation tools (Layer 4):
 *   ue_validate_references  — broken hard-dependency check
 *   ue_validate_naming      — asset naming convention check
 *   ue_validate_world_rules — scene rule validation
 *   ue_evaluate_goal        — goal assertion evaluation
 *
 * All read-only. All require GameThread execution.
 * Declaration only — implementation in MCPTool_Validation.cpp
 */
namespace MCPTool_Validation
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
