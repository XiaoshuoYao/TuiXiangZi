// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MCPToolRegistry.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Services/MCPSnapshotStore.h"

class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;
class FMCPSnapshotStore;

/**
 * Snapshot tools: ue_snapshot_build, ue_snapshot_list, ue_snapshot_query
 *
 * Aggregation layer that collects project state snapshots by calling into
 * existing Asset/Reflection/Blueprint/Material tool logic.
 * All require GameThread execution.
 * Implementation in MCPTool_Snapshots.cpp
 */
namespace MCPTool_Snapshots
{
	void RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore, FMCPSnapshotStore& SnapshotStore);
}
