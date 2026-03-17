// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;

/**
 * Blueprint default-value editing tools (Editor-only):
 *   ue_get_blueprint_edit_schema, ue_get_blueprint_defaults,
 *   ue_set_blueprint_defaults, ue_set_blueprint_component_defaults,
 *   ue_compile_blueprint
 *
 * Implementation in MCPTool_BlueprintEdit.cpp
 */
namespace MCPTool_BlueprintEdit
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore);
}
