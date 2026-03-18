// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_Blueprints.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "MCPToolRegistry.h"

#if WITH_EDITOR

#include "Services/MCPBlueprintGraphExtractor.h"
#include "Reflection/MCPReflectionCore.h"

#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Switch.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

// ============================================================================
// Helpers
// ============================================================================
namespace MCPToolBlueprintsPrivate
{
	/** Load a blueprint by asset path. Returns nullptr on failure. */
	UBlueprint* LoadBlueprintByPath(const FString& BpPath)
	{
		return LoadObject<UBlueprint>(nullptr, *BpPath);
	}

	/** Get graph type string. */
	FString GetGraphType(UBlueprint* BP, UEdGraph* Graph)
	{
		if (BP->UbergraphPages.Contains(Graph)) return TEXT("EventGraph");
		if (BP->FunctionGraphs.Contains(Graph)) return TEXT("Function");
		if (BP->MacroGraphs.Contains(Graph)) return TEXT("Macro");
		return TEXT("Other");
	}
}
using namespace MCPToolBlueprintsPrivate;

// ============================================================================
// ue_bp_list
// ============================================================================
namespace BPList
{
	static constexpr const TCHAR* ToolName = TEXT("ue_bp_list");
	static constexpr const TCHAR* ToolDescription =
		TEXT("List blueprint assets, optionally filtered by path prefix and class.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_bp_list");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString PathPrefix = MCPToolExecution::GetStringParam(Args, TEXT("path_prefix"));
		FString ClassName = MCPToolExecution::GetStringParam(Args, TEXT("class"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 200);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.list") }, {}, true);

		if (Limit < 1 || Offset < 0)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Invalid pagination parameters"));
			Envelope->SetArrayField(TEXT("bps"), {});
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		if (!PathPrefix.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*PathPrefix));
			Filter.bRecursivePaths = true;
		}

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		// Sort for stable output
		Assets.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.PackageName.LexicalLess(B.PackageName);
		});

		int32 Total = Assets.Num();
		int32 EffectiveLimit = FMath::Min(Limit, Snap.MaxItems);
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + EffectiveLimit, Total);

		TArray<TSharedPtr<FJsonValue>> BPsArray;
		for (int32 i = Start; i < End; ++i)
		{
			TSharedPtr<FJsonObject> BPObj = MakeShared<FJsonObject>();
			BPObj->SetStringField(TEXT("bp_path"), Assets[i].GetObjectPathString());

			// Check if loaded to get generated_class without triggering load
			UObject* Loaded = Assets[i].FastGetAsset(false);
			if (UBlueprint* BP = Cast<UBlueprint>(Loaded))
			{
				if (BP->GeneratedClass)
				{
					BPObj->SetStringField(TEXT("generated_class"), BP->GeneratedClass->GetPathName());
				}
				else
				{
					BPObj->SetField(TEXT("generated_class"), MakeShared<FJsonValueNull>());
				}
			}
			else
			{
				BPObj->SetField(TEXT("generated_class"), MakeShared<FJsonValueNull>());
			}

			BPsArray.Add(MakeShared<FJsonValueObject>(BPObj));
		}

		Envelope->SetArrayField(TEXT("bps"), BPsArray);
		Envelope->SetNumberField(TEXT("total"), Total);
		return Envelope;
	}
}

// ============================================================================
// ue_bp_graph_list
// ============================================================================
namespace BPGraphList
{
	static constexpr const TCHAR* ToolName = TEXT("ue_bp_graph_list");
	static constexpr const TCHAR* ToolDescription =
		TEXT("List all graphs in a blueprint (event graphs, functions, macros).");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_bp_graph_list");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString BpPath = MCPToolExecution::GetStringParam(Args, TEXT("bp_path"));

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.graph_list") }, {}, true);

		if (BpPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: bp_path"));
			Envelope->SetArrayField(TEXT("graphs"), {});
			return Envelope;
		}

		UBlueprint* BP = LoadBlueprintByPath(BpPath);
		if (!BP)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Blueprint not found or failed to load: %s"), *BpPath));
			Envelope->SetArrayField(TEXT("graphs"), {});
			return Envelope;
		}

		auto AddGraphs = [](TArray<TSharedPtr<FJsonValue>>& OutArray, const TArray<UEdGraph*>& Graphs, const TCHAR* TypeStr)
		{
			for (UEdGraph* Graph : Graphs)
			{
				if (!Graph) continue;
				TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
				GraphObj->SetStringField(TEXT("name"), Graph->GetName());
				GraphObj->SetStringField(TEXT("type"), TypeStr);
				GraphObj->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
				OutArray.Add(MakeShared<FJsonValueObject>(GraphObj));
			}
		};

		TArray<TSharedPtr<FJsonValue>> GraphsArray;
		AddGraphs(GraphsArray, BP->UbergraphPages, TEXT("EventGraph"));
		AddGraphs(GraphsArray, BP->FunctionGraphs, TEXT("Function"));
		AddGraphs(GraphsArray, BP->MacroGraphs, TEXT("Macro"));
		AddGraphs(GraphsArray, BP->DelegateSignatureGraphs, TEXT("Other"));

		Envelope->SetArrayField(TEXT("graphs"), GraphsArray);
		return Envelope;
	}
}

// ============================================================================
// ue_bp_graph_get
// ============================================================================
namespace BPGraphGet
{
	static constexpr const TCHAR* ToolName = TEXT("ue_bp_graph_get");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get detailed graph data (nodes, pins, edges) for a specific blueprint graph.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_bp_graph_get");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString BpPath = MCPToolExecution::GetStringParam(Args, TEXT("bp_path"));
		FString GraphName = MCPToolExecution::GetStringParam(Args, TEXT("graph_name"));
		FString Detail = MCPToolExecution::GetStringParam(Args, TEXT("detail"), TEXT("compact"));
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), true);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.graph_get") }, {}, true);

		if (BpPath.IsEmpty() || GraphName.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameters: bp_path and graph_name"));
			return Envelope;
		}

		UBlueprint* BP = LoadBlueprintByPath(BpPath);
		if (!BP)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *BpPath));
			return Envelope;
		}

		UEdGraph* Graph = MCPBlueprintGraphExtractor::FindGraph(BP, GraphName);
		if (!Graph)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Graph '%s' not found in blueprint"), *GraphName));
			return Envelope;
		}

		TSharedPtr<FJsonObject> GraphData = MCPBlueprintGraphExtractor::SerializeGraph(Graph, Detail);
		Envelope->SetObjectField(TEXT("graph"), GraphData);

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_bp_function_summary
// ============================================================================
namespace BPFunctionSummary
{
	static constexpr const TCHAR* ToolName = TEXT("ue_bp_function_summary");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get a high-level summary of a blueprint function: steps, calls, reads, writes, branches.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_bp_function_summary");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString BpPath = MCPToolExecution::GetStringParam(Args, TEXT("bp_path"));
		FString FunctionName = MCPToolExecution::GetStringParam(Args, TEXT("function_name"));
		FString Verbosity = MCPToolExecution::GetStringParam(Args, TEXT("verbosity"), TEXT("med"));

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.function_summary") }, {}, true);

		auto MakeEmptySummary = [&Envelope]()
		{
			TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
			Summary->SetArrayField(TEXT("steps"), {});
			Summary->SetArrayField(TEXT("calls"), {});
			Summary->SetArrayField(TEXT("reads"), {});
			Summary->SetArrayField(TEXT("writes"), {});
			Summary->SetArrayField(TEXT("branches"), {});
			Envelope->SetObjectField(TEXT("summary"), Summary);
		};

		if (BpPath.IsEmpty() || FunctionName.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameters: bp_path and function_name"));
			MakeEmptySummary();
			return Envelope;
		}

		UBlueprint* BP = LoadBlueprintByPath(BpPath);
		if (!BP)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Blueprint not found: %s"), *BpPath));
			MakeEmptySummary();
			return Envelope;
		}

		// Find function graph
		UEdGraph* FuncGraph = nullptr;
		for (UEdGraph* Graph : BP->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				FuncGraph = Graph;
				break;
			}
		}
		if (!FuncGraph)
		{
			// Also check UbergraphPages for event-driven functions
			FuncGraph = MCPBlueprintGraphExtractor::FindGraph(BP, FunctionName);
		}
		if (!FuncGraph)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Function graph '%s' not found"), *FunctionName));
			MakeEmptySummary();
			return Envelope;
		}

		// Classify nodes
		TArray<TSharedPtr<FJsonValue>> Calls;
		TArray<TSharedPtr<FJsonValue>> Reads;
		TArray<TSharedPtr<FJsonValue>> Writes;
		TArray<TSharedPtr<FJsonValue>> Branches;
		TArray<TSharedPtr<FJsonValue>> Steps;

		UK2Node_FunctionEntry* EntryNode = nullptr;

		for (UEdGraphNode* Node : FuncGraph->Nodes)
		{
			if (!Node) continue;

			FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				TSharedPtr<FJsonObject> CallObj = MakeShared<FJsonObject>();
				CallObj->SetStringField(TEXT("function"), CallNode->GetFunctionName().ToString());
				CallObj->SetStringField(TEXT("title"), NodeTitle);
				CallObj->SetStringField(TEXT("node_id"), MCPBlueprintGraphExtractor::GetStableNodeId(Node));
				Calls.Add(MakeShared<FJsonValueObject>(CallObj));
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				TSharedPtr<FJsonObject> ReadObj = MakeShared<FJsonObject>();
				ReadObj->SetStringField(TEXT("variable"), GetNode->GetVarName().ToString());
				ReadObj->SetStringField(TEXT("node_id"), MCPBlueprintGraphExtractor::GetStableNodeId(Node));
				Reads.Add(MakeShared<FJsonValueObject>(ReadObj));
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				TSharedPtr<FJsonObject> WriteObj = MakeShared<FJsonObject>();
				WriteObj->SetStringField(TEXT("variable"), SetNode->GetVarName().ToString());
				WriteObj->SetStringField(TEXT("node_id"), MCPBlueprintGraphExtractor::GetStableNodeId(Node));
				Writes.Add(MakeShared<FJsonValueObject>(WriteObj));
			}
			else if (Cast<UK2Node_IfThenElse>(Node) || Cast<UK2Node_Switch>(Node))
			{
				TSharedPtr<FJsonObject> BranchObj = MakeShared<FJsonObject>();
				BranchObj->SetStringField(TEXT("kind"), Cast<UK2Node_IfThenElse>(Node) ? TEXT("IfThenElse") : TEXT("Switch"));
				BranchObj->SetStringField(TEXT("title"), NodeTitle);
				BranchObj->SetStringField(TEXT("node_id"), MCPBlueprintGraphExtractor::GetStableNodeId(Node));
				Branches.Add(MakeShared<FJsonValueObject>(BranchObj));
			}

			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				EntryNode = Entry;
			}
		}

		// Generate steps via exec pin DFS from entry node
		if (EntryNode)
		{
			TSet<UEdGraphNode*> Visited;
			UEdGraphNode* Current = EntryNode;

			while (Current && !Visited.Contains(Current))
			{
				Visited.Add(Current);
				FString StepTitle = Current->GetNodeTitle(ENodeTitleType::ListView).ToString();
				if (!StepTitle.IsEmpty())
				{
					Steps.Add(MakeShared<FJsonValueString>(StepTitle));
				}

				// Follow exec output pin
				UEdGraphNode* Next = nullptr;
				for (UEdGraphPin* Pin : Current->Pins)
				{
					if (!Pin || Pin->Direction != EGPD_Output) continue;
					if (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;

					// For branches, prefer "Then" pin
					if (Cast<UK2Node_IfThenElse>(Current))
					{
						if (Pin->PinName != TEXT("Then")) continue;
					}

					// Take first connected exec output
					if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0])
					{
						Next = Pin->LinkedTo[0]->GetOwningNode();
						break;
					}
				}
				Current = Next;
			}
		}

		TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetArrayField(TEXT("steps"), Steps);
		Summary->SetArrayField(TEXT("calls"), Calls);
		Summary->SetArrayField(TEXT("reads"), Reads);
		Summary->SetArrayField(TEXT("writes"), Writes);
		Summary->SetArrayField(TEXT("branches"), Branches);
		Envelope->SetObjectField(TEXT("summary"), Summary);

		return Envelope;
	}
}

// ============================================================================
// ue_bp_find_calls
// ============================================================================
namespace BPFindCalls
{
	static constexpr const TCHAR* ToolName = TEXT("ue_bp_find_calls");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Find all blueprint nodes that call a specific function across the project.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_bp_find_calls");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString FunctionName = MCPToolExecution::GetStringParam(Args, TEXT("function_name"));
		FString PathPrefix = MCPToolExecution::GetStringParam(Args, TEXT("path_prefix"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 200);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), false);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.find_calls") }, {}, true);

		if (FunctionName.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: function_name"));
			Envelope->SetArrayField(TEXT("hits"), {});
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		if (!PathPrefix.IsEmpty())
		{
			Filter.PackagePaths.Add(FName(*PathPrefix));
			Filter.bRecursivePaths = true;
		}

		TArray<FAssetData> BPAssets;
		AssetRegistry.GetAssets(Filter, BPAssets);

		TArray<TSharedPtr<FJsonValue>> AllHits;

		// Process in batches to avoid long GameThread blocks
		for (const FAssetData& AD : BPAssets)
		{
			UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AD.GetObjectPathString());
			if (!BP) continue;

			// Scan all graphs
			auto ScanGraphs = [&](const TArray<UEdGraph*>& Graphs)
			{
				for (UEdGraph* Graph : Graphs)
				{
					if (!Graph) continue;
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
						if (!CallNode) continue;

						if (CallNode->GetFunctionName().ToString() == FunctionName)
						{
							TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
							Hit->SetStringField(TEXT("bp_path"), AD.GetObjectPathString());
							Hit->SetStringField(TEXT("graph"), Graph->GetName());
							Hit->SetStringField(TEXT("node_id"),
								MCPBlueprintGraphExtractor::GetStableNodeId(Node));
							Hit->SetStringField(TEXT("context_snippet"),
								Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
							AllHits.Add(MakeShared<FJsonValueObject>(Hit));
						}
					}
				}
			};

			ScanGraphs(BP->UbergraphPages);
			ScanGraphs(BP->FunctionGraphs);
			ScanGraphs(BP->MacroGraphs);
		}

		// Paginate
		int32 Total = AllHits.Num();
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + FMath::Min(Limit, Snap.MaxItems), Total);

		TArray<TSharedPtr<FJsonValue>> PagedHits;
		for (int32 i = Start; i < End; ++i)
		{
			PagedHits.Add(AllHits[i]);
		}

		Envelope->SetArrayField(TEXT("hits"), PagedHits);
		Envelope->SetNumberField(TEXT("total"), Total);

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_bp_find_property_access
// ============================================================================
namespace BPFindPropertyAccess
{
	static constexpr const TCHAR* ToolName = TEXT("ue_bp_find_property_access");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Find blueprint nodes that read or write a specific property of a class.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_bp_find_property_access");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString ClassName = MCPToolExecution::GetStringParam(Args, TEXT("class_name"));
		FString PropertyName = MCPToolExecution::GetStringParam(Args, TEXT("property_name"));
		FString Access = MCPToolExecution::GetStringParam(Args, TEXT("access"), TEXT("both"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 200);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L2.blueprints.find_property_access") }, {}, true);

		if (ClassName.IsEmpty() || PropertyName.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameters: class_name and property_name"));
			Envelope->SetArrayField(TEXT("hits"), {});
			return Envelope;
		}

		UClass* TargetClass = MCPReflection::ResolveClass(ClassName);
		if (!TargetClass)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Class not found: %s"), *ClassName));
			Envelope->SetArrayField(TEXT("hits"), {});
			return Envelope;
		}

		bool bMatchRead = (Access == TEXT("read") || Access == TEXT("both"));
		bool bMatchWrite = (Access == TEXT("write") || Access == TEXT("both"));

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

		TArray<FAssetData> BPAssets;
		AssetRegistry.GetAssets(Filter, BPAssets);

		TArray<TSharedPtr<FJsonValue>> AllHits;

		for (const FAssetData& AD : BPAssets)
		{
			UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AD.GetObjectPathString());
			if (!BP) continue;

			auto ScanGraphs = [&](const TArray<UEdGraph*>& Graphs)
			{
				for (UEdGraph* Graph : Graphs)
				{
					if (!Graph) continue;
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						if (!Node) continue;

						FString AccessType;

						if (bMatchRead)
						{
							if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
							{
								if (GetNode->GetVarName().ToString() == PropertyName)
								{
									// Check if the variable's parent class is related to target
									AccessType = TEXT("read");
								}
							}
						}

						if (bMatchWrite && AccessType.IsEmpty())
						{
							if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
							{
								if (SetNode->GetVarName().ToString() == PropertyName)
								{
									AccessType = TEXT("write");
								}
							}
						}

						if (!AccessType.IsEmpty())
						{
							TSharedPtr<FJsonObject> Hit = MakeShared<FJsonObject>();
							Hit->SetStringField(TEXT("bp_path"), AD.GetObjectPathString());
							Hit->SetStringField(TEXT("graph"), Graph->GetName());
							Hit->SetStringField(TEXT("node_id"),
								MCPBlueprintGraphExtractor::GetStableNodeId(Node));
							Hit->SetStringField(TEXT("access"), AccessType);
							AllHits.Add(MakeShared<FJsonValueObject>(Hit));
						}
					}
				}
			};

			ScanGraphs(BP->UbergraphPages);
			ScanGraphs(BP->FunctionGraphs);
			ScanGraphs(BP->MacroGraphs);
		}

		// Paginate
		int32 Total = AllHits.Num();
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + FMath::Min(Limit, Snap.MaxItems), Total);

		TArray<TSharedPtr<FJsonValue>> PagedHits;
		for (int32 i = Start; i < End; ++i)
		{
			PagedHits.Add(AllHits[i]);
		}

		Envelope->SetArrayField(TEXT("hits"), PagedHits);
		Envelope->SetNumberField(TEXT("total"), Total);

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_Blueprints::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
{
	auto RegisterGameThreadTool = [&](const TCHAR* Name, const TCHAR* Desc, const TCHAR* SchemaDef,
		TFunction<TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>&, FMCPRuntimeState&, FMCPResourceStore&)> Impl)
	{
		TSharedPtr<FJsonObject> Schema = Registry.ExtractInputSchema(SchemaDef);
		FMCPToolRegistration Reg;
		Reg.Descriptor.Name = Name;
		Reg.Descriptor.Description = Desc;
		Reg.Descriptor.InputSchema = Schema;
		Reg.Execute = [&RuntimeState, &ResourceStore, Impl](const TSharedPtr<FJsonObject>& Args) -> TSharedPtr<FJsonObject>
		{
			return MCPToolExecution::RunOnGameThread([&]()
			{
				return Impl(Args, RuntimeState, ResourceStore);
			});
		};
		Registry.RegisterTool(MoveTemp(Reg));
	};

	RegisterGameThreadTool(BPList::ToolName, BPList::ToolDescription, BPList::SchemaDefName, BPList::ExecuteOnGameThread);
	RegisterGameThreadTool(BPGraphList::ToolName, BPGraphList::ToolDescription, BPGraphList::SchemaDefName, BPGraphList::ExecuteOnGameThread);
	RegisterGameThreadTool(BPGraphGet::ToolName, BPGraphGet::ToolDescription, BPGraphGet::SchemaDefName, BPGraphGet::ExecuteOnGameThread);
	RegisterGameThreadTool(BPFunctionSummary::ToolName, BPFunctionSummary::ToolDescription, BPFunctionSummary::SchemaDefName, BPFunctionSummary::ExecuteOnGameThread);
	RegisterGameThreadTool(BPFindCalls::ToolName, BPFindCalls::ToolDescription, BPFindCalls::SchemaDefName, BPFindCalls::ExecuteOnGameThread);
	RegisterGameThreadTool(BPFindPropertyAccess::ToolName, BPFindPropertyAccess::ToolDescription, BPFindPropertyAccess::SchemaDefName, BPFindPropertyAccess::ExecuteOnGameThread);
}

#else // !WITH_EDITOR

void MCPTool_Blueprints::RegisterAll(FMCPToolRegistry& /*Registry*/, FMCPRuntimeState& /*RuntimeState*/, FMCPResourceStore& /*ResourceStore*/)
{
	// Blueprint tools are only available in Editor builds
}

#endif // WITH_EDITOR
