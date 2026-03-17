// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#if WITH_EDITOR

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * Blueprint graph serialization utilities.
 * Converts UEdGraph data to JSON for MCP tool responses.
 * All functions must be called on the GameThread.
 */
namespace MCPBlueprintGraphExtractor
{
	/** Find a graph by name within a blueprint. Returns nullptr if not found. */
	UEdGraph* FindGraph(UBlueprint* BP, const FString& GraphName);

	/**
	 * Serialize a UEdGraph to GraphData JSON (nodes/pins/edges).
	 * @param Detail "compact" = id/kind/title/pos/pins/edges; "full" adds comment/attributes/defaults
	 */
	TSharedPtr<FJsonObject> SerializeGraph(UEdGraph* Graph, const FString& Detail);

	/** Return a stable node ID using NodeGuid. */
	FString GetStableNodeId(UEdGraphNode* Node);

	/** Return a stable pin ID: PersistentGuid if valid, else fallback format. */
	FString GetStablePinId(UEdGraphPin* Pin);
}

#endif // WITH_EDITOR
