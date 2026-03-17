// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/MCPBlueprintGraphExtractor.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

FString MCPBlueprintGraphExtractor::GetStableNodeId(UEdGraphNode* Node)
{
	if (!Node) return TEXT("");
	return Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
}

FString MCPBlueprintGraphExtractor::GetStablePinId(UEdGraphPin* Pin)
{
	if (!Pin) return TEXT("");

	if (Pin->PersistentGuid.IsValid())
	{
		return Pin->PersistentGuid.ToString(EGuidFormats::DigitsWithHyphens);
	}

	// Fallback: NodeGuid:PinName:Direction
	FString DirStr = (Pin->Direction == EGPD_Input) ? TEXT("input") : TEXT("output");
	FString NodeId = Pin->GetOwningNode() ? GetStableNodeId(Pin->GetOwningNode()) : TEXT("unknown");
	return FString::Printf(TEXT("%s:%s:%s"), *NodeId, *Pin->PinName.ToString(), *DirStr);
}

UEdGraph* MCPBlueprintGraphExtractor::FindGraph(UBlueprint* BP, const FString& GraphName)
{
	if (!BP) return nullptr;

	auto SearchGraphs = [&GraphName](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				return Graph;
			}
		}
		return nullptr;
	};

	if (UEdGraph* G = SearchGraphs(BP->UbergraphPages)) return G;
	if (UEdGraph* G = SearchGraphs(BP->FunctionGraphs)) return G;
	if (UEdGraph* G = SearchGraphs(BP->MacroGraphs)) return G;
	if (UEdGraph* G = SearchGraphs(BP->DelegateSignatureGraphs)) return G;

	return nullptr;
}

TSharedPtr<FJsonObject> MCPBlueprintGraphExtractor::SerializeGraph(UEdGraph* Graph, const FString& Detail)
{
	if (!Graph) return nullptr;

	bool bFull = Detail.Equals(TEXT("full"), ESearchCase::IgnoreCase);

	TSharedPtr<FJsonObject> GraphData = MakeShared<FJsonObject>();
	GraphData->SetStringField(TEXT("name"), Graph->GetName());

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	TArray<TSharedPtr<FJsonValue>> EdgesArray;

	// Track processed edges to avoid duplicates
	TSet<FString> ProcessedEdges;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("id"), GetStableNodeId(Node));
		NodeObj->SetStringField(TEXT("kind"), Node->GetClass()->GetName());
		NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

		// Position
		TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
		PosObj->SetNumberField(TEXT("x"), Node->NodePosX);
		PosObj->SetNumberField(TEXT("y"), Node->NodePosY);
		NodeObj->SetObjectField(TEXT("pos"), PosObj);

		// Full detail extras
		if (bFull)
		{
			if (!Node->NodeComment.IsEmpty())
			{
				NodeObj->SetStringField(TEXT("comment"), Node->NodeComment);
			}
		}

		// Pins
		TArray<TSharedPtr<FJsonValue>> PinsArray;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) continue;

			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			FString PinId = GetStablePinId(Pin);
			PinObj->SetStringField(TEXT("id"), PinId);
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"),
				Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
			PinObj->SetStringField(TEXT("pin_type"), Pin->PinType.PinCategory.ToString());

			if (bFull && !Pin->DefaultValue.IsEmpty())
			{
				PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
			}

			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));

			// Collect edges (from output pins to avoid duplicates)
			if (Pin->Direction == EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin) continue;
					FString FromId = PinId;
					FString ToId = GetStablePinId(LinkedPin);
					FString EdgeKey = FString::Printf(TEXT("%s->%s"), *FromId, *ToId);

					if (!ProcessedEdges.Contains(EdgeKey))
					{
						ProcessedEdges.Add(EdgeKey);
						TSharedPtr<FJsonObject> EdgeObj = MakeShared<FJsonObject>();
						EdgeObj->SetStringField(TEXT("from_pin"), FromId);
						EdgeObj->SetStringField(TEXT("to_pin"), ToId);
						EdgesArray.Add(MakeShared<FJsonValueObject>(EdgeObj));
					}
				}
			}
		}
		NodeObj->SetArrayField(TEXT("pins"), PinsArray);

		NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	GraphData->SetArrayField(TEXT("nodes"), NodesArray);
	GraphData->SetArrayField(TEXT("edges"), EdgesArray);

	return GraphData;
}

#endif // WITH_EDITOR
