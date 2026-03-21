// Copyright Epic Games, Inc. All Rights Reserved.

#include "UE5MCP.h"
#include "MCPToolRegistry.h"
#include "Tools/MCPTool_ClassSchema.h"
#include "Tools/MCPTool_Core.h"
#include "Tools/MCPTool_Assets.h"
#include "Tools/MCPTool_World.h"
#include "Tools/MCPTool_Blueprints.h"
#include "Tools/MCPTool_Materials.h"
#include "Tools/MCPTool_Snapshots.h"
#include "Tools/MCPTool_BlueprintEdit.h"
#include "Tools/MCPTool_AssetEdit.h"
#include "Tools/MCPTool_DataTable.h"
#include "Tools/MCPTool_DataAsset.h"
#include "Tools/MCPTool_Validation.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Services/MCPLogBuffer.h"
#include "Services/MCPSnapshotStore.h"
#include "Services/MCPToolMetrics.h"

// HTTP Server
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpPath.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"

// JSON
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

// Plugin API
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FUE5MCPModule"

DEFINE_LOG_CATEGORY_STATIC(LogUE5MCP, Log, All);

// ============================================================================
// Constants
// ============================================================================

namespace MCPConstants
{
	static const FString ProtocolVersion = TEXT("2025-03-26");
	static const FString ServerName      = TEXT("ue5-mcp");
	static const FString ServerVersion   = TEXT("0.1.0");
}

// ============================================================================
// HTTP State (PIMPL)
// ============================================================================

struct FUE5MCPModule::FHttpState
{
	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle PostRouteHandle;
	FHttpRouteHandle GetRouteHandle;
	uint32 Port = 0;
};

// ============================================================================
// Helpers
// ============================================================================

namespace
{
	FString JsonToString(const TSharedPtr<FJsonObject>& JsonObject)
	{
		FString Output;
		auto Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		return Output;
	}

	TArray<uint8> StringToUtf8Bytes(const FString& Str)
	{
		TArray<uint8> Bytes;
		FTCHARToUTF8 Converter(*Str);
		Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
		return Bytes;
	}

	TSharedPtr<FJsonValue> ExtractId(const TSharedPtr<FJsonObject>& Request)
	{
		if (Request.IsValid() && Request->HasField(TEXT("id")))
		{
			return Request->TryGetField(TEXT("id"));
		}
		return MakeShared<FJsonValueNull>();
	}
}

// ============================================================================
// Module Lifecycle
// ============================================================================

void FUE5MCPModule::StartupModule()
{
	// Skip entirely when running as a commandlet (cooking, packaging, etc.)
	if (IsRunningCommandlet())
	{
		UE_LOG(LogUE5MCP, Log, TEXT("Running in commandlet mode — skipping MCP server startup"));
		return;
	}

	// Create runtime services
	RuntimeState = MakeUnique<FMCPRuntimeState>();
	ResourceStore = MakeUnique<FMCPResourceStore>();
	LogBuffer = MakeUnique<FMCPLogBuffer>();
	SnapshotStore = MakeUnique<FMCPSnapshotStore>();
	ToolMetrics = MakeUnique<FMCPToolMetrics>();

	// Register log buffer as output device
	GLog->AddOutputDevice(LogBuffer.Get());

	// Load existing snapshot metadata from disk
	SnapshotStore->LoadMetaFromDisk();

	// Load historical tool metrics from disk
	ToolMetrics->LoadFromDisk();

	InitializeToolRegistry();
	RegisterMethodHandlers();
	StartHttpServer();
}

void FUE5MCPModule::ShutdownModule()
{
	// Flush tool metrics synchronously before teardown
	if (ToolMetrics.IsValid())
	{
		ToolMetrics->Shutdown();
	}

	StopHttpServer();
	MethodHandlers.Empty();
	ToolRegistry.Reset();

	// Remove log buffer before destroying it
	if (LogBuffer.IsValid())
	{
		GLog->RemoveOutputDevice(LogBuffer.Get());
	}

	LogBuffer.Reset();
	SnapshotStore.Reset();
	ToolMetrics.Reset();
	ResourceStore.Reset();
	RuntimeState.Reset();
}

// ============================================================================
// HTTP Server
// ============================================================================

void FUE5MCPModule::StartHttpServer()
{
	HttpState = MakeUnique<FHttpState>();

	for (uint32 i = 0; i < MaxPortRetries; ++i)
	{
		const uint32 Port = DefaultPort + i;
		TSharedPtr<IHttpRouter> Router = FHttpServerModule::Get().GetHttpRouter(Port, true);

		if (!Router.IsValid())
		{
			UE_LOG(LogUE5MCP, Warning, TEXT("Port %d unavailable, trying next..."), Port);
			continue;
		}

		HttpState->Router = Router;
		HttpState->Port = Port;

		// POST /mcp — main JSON-RPC entry point
		HttpState->PostRouteHandle = Router->BindRoute(
			FHttpPath(TEXT("/mcp")),
			EHttpServerRequestVerbs::VERB_POST,
			FHttpRequestHandler::CreateLambda(
				[this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete) -> bool
				{
					// Require application/json Content-Type
					bool bHasJson = false;
					const TArray<FString>* ContentTypeValues = Request.Headers.Find(TEXT("content-type"));
					if (!ContentTypeValues)
					{
						ContentTypeValues = Request.Headers.Find(TEXT("Content-Type"));
					}
					if (ContentTypeValues)
					{
						for (const FString& Val : *ContentTypeValues)
						{
							if (Val.Contains(TEXT("application/json")))
							{
								bHasJson = true;
								break;
							}
						}
					}

					if (!bHasJson)
					{
						FString Body = TEXT(R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Content-Type must be application/json"}})");
						auto Response = FHttpServerResponse::Create(StringToUtf8Bytes(Body), TEXT("application/json"));
						Response->Code = static_cast<EHttpServerResponseCodes>(415);
						OnComplete(MoveTemp(Response));
						return true;
					}

					// Read request body as UTF-8 string
					FUTF8ToTCHAR BodyConv(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
					FString BodyStr(BodyConv.Length(), BodyConv.Get());

					FString ResponseBody = ProcessJsonRpcRequest(BodyStr);
					auto Response = FHttpServerResponse::Create(StringToUtf8Bytes(ResponseBody), TEXT("application/json"));
					Response->Code = EHttpServerResponseCodes::Ok;
					OnComplete(MoveTemp(Response));
					return true;
				}
			)
		);

		// GET /mcp — 405 Method Not Allowed
		HttpState->GetRouteHandle = Router->BindRoute(
			FHttpPath(TEXT("/mcp")),
			EHttpServerRequestVerbs::VERB_GET,
			FHttpRequestHandler::CreateLambda(
				[](const FHttpServerRequest& /*Request*/, const FHttpResultCallback& OnComplete) -> bool
				{
					FString Body = TEXT(R"({"jsonrpc":"2.0","id":null,"error":{"code":-32600,"message":"Method Not Allowed. Use POST."}})");
					auto Response = FHttpServerResponse::Create(StringToUtf8Bytes(Body), TEXT("application/json"));
					Response->Code = static_cast<EHttpServerResponseCodes>(405);
					OnComplete(MoveTemp(Response));
					return true;
				}
			)
		);

		FHttpServerModule::Get().StartAllListeners();
		if (RuntimeState.IsValid())
		{
			RuntimeState->SetPort(Port);
		}
		UE_LOG(LogUE5MCP, Log, TEXT("MCP server listening on http://127.0.0.1:%d/mcp"), Port);
		return;
	}

	UE_LOG(LogUE5MCP, Error, TEXT("Failed to start MCP HTTP server after %d port attempts (tried %d-%d)"),
		MaxPortRetries, DefaultPort, DefaultPort + MaxPortRetries - 1);
}

void FUE5MCPModule::StopHttpServer()
{
	if (HttpState.IsValid() && HttpState->Router.IsValid())
	{
		HttpState->Router->UnbindRoute(HttpState->PostRouteHandle);
		HttpState->Router->UnbindRoute(HttpState->GetRouteHandle);
		FHttpServerModule::Get().StopAllListeners();
		UE_LOG(LogUE5MCP, Log, TEXT("MCP server stopped (was on port %d)"), HttpState->Port);
	}
	HttpState.Reset();
}

// ============================================================================
// JSON-RPC Dispatch
// ============================================================================

FString FUE5MCPModule::ProcessJsonRpcRequest(const FString& RequestBody)
{
	// 1. Parse JSON
	TSharedPtr<FJsonObject> RequestObj;
	auto Reader = TJsonReaderFactory<>::Create(RequestBody);
	if (!FJsonSerializer::Deserialize(Reader, RequestObj) || !RequestObj.IsValid())
	{
		return MakeErrorResponse(MakeShared<FJsonValueNull>(), -32700, TEXT("Parse error"));
	}

	TSharedPtr<FJsonValue> IdValue = ExtractId(RequestObj);

	// 2. Validate jsonrpc version
	FString JsonRpcVersion;
	if (!RequestObj->TryGetStringField(TEXT("jsonrpc"), JsonRpcVersion) || JsonRpcVersion != TEXT("2.0"))
	{
		return MakeErrorResponse(IdValue, -32600, TEXT("Invalid Request: jsonrpc must be \"2.0\""));
	}

	// 3. Validate method
	FString Method;
	if (!RequestObj->TryGetStringField(TEXT("method"), Method) || Method.IsEmpty())
	{
		return MakeErrorResponse(IdValue, -32600, TEXT("Invalid Request: missing or empty method"));
	}

	// 4. Extract params (optional, default to empty object)
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	{
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		if (RequestObj->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr)
		{
			Params = *ParamsPtr;
		}
	}

	// 5. Dispatch
	FMethodHandler* Handler = MethodHandlers.Find(Method);
	if (!Handler)
	{
		return MakeErrorResponse(IdValue, -32601, FString::Printf(TEXT("Method not found: %s"), *Method));
	}

	FMCPMethodResult MethodResult = (*Handler)(Params);

	if (MethodResult.IsError())
	{
		return MakeErrorResponse(IdValue, MethodResult.ErrorCode, MethodResult.ErrorMessage);
	}

	return MakeSuccessResponse(IdValue, MethodResult.Result);
}

// ============================================================================
// Tool Registry Initialization
// ============================================================================

void FUE5MCPModule::InitializeToolRegistry()
{
	ToolRegistry = MakeUnique<FMCPToolRegistry>();

	// Locate schema file via plugin Content directory
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE5MCP"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogUE5MCP, Error, TEXT("Could not find UE5MCP plugin via IPluginManager"));
		return;
	}

	FString SchemaPath = Plugin->GetContentDir() / TEXT("Resources") / TEXT("ue_mcp_schema_fixed.json");

	if (!ToolRegistry->LoadSchemaFile(SchemaPath))
	{
		UE_LOG(LogUE5MCP, Error, TEXT("Schema file load failed, no tools will be registered"));
		return;
	}

	// Register tools
	MCPTool_ClassSchema::Register(*ToolRegistry);

	// Register Core tools (ping, capabilities, settings, log_tail)
	if (RuntimeState.IsValid() && LogBuffer.IsValid())
	{
		MCPTool_Core::RegisterAll(*ToolRegistry, *RuntimeState, *LogBuffer);
	}

	// Register Asset tools
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_Assets::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register World/Reflection tools
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_World::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register Blueprint tools (Editor-only)
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_Blueprints::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register Blueprint Edit tools (defaults editing, compile)
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_BlueprintEdit::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register Material tools (Editor-only)
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_Materials::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register Asset Edit tools (create, duplicate, rename, move, save, delete, undo, redo)
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_AssetEdit::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register Snapshot tools
	if (RuntimeState.IsValid() && ResourceStore.IsValid() && SnapshotStore.IsValid())
	{
		MCPTool_Snapshots::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore, *SnapshotStore);
	}

	// Register DataTable tools
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_DataTable::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register DataAsset tools
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_DataAsset::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	// Register Validation / Evaluation tools (L4)
	if (RuntimeState.IsValid() && ResourceStore.IsValid())
	{
		MCPTool_Validation::RegisterAll(*ToolRegistry, *RuntimeState, *ResourceStore);
	}

	UE_LOG(LogUE5MCP, Log, TEXT("Tool registry initialized: %d tool(s) registered"), ToolRegistry->Num());
}

// ============================================================================
// Method Registration
// ============================================================================

void FUE5MCPModule::RegisterMethodHandlers()
{
	MethodHandlers.Add(TEXT("initialize"),
		FMethodHandler([this](const TSharedPtr<FJsonObject>& P) { return HandleInitialize(P); }));

	MethodHandlers.Add(TEXT("tools/list"),
		FMethodHandler([this](const TSharedPtr<FJsonObject>& P) { return HandleToolsList(P); }));

	MethodHandlers.Add(TEXT("tools/call"),
		FMethodHandler([this](const TSharedPtr<FJsonObject>& P) { return HandleToolsCall(P); }));

	MethodHandlers.Add(TEXT("resources/read"),
		FMethodHandler([this](const TSharedPtr<FJsonObject>& P) { return HandleResourcesRead(P); }));
}

// ============================================================================
// MCP Method Implementations
// ============================================================================

FMCPMethodResult FUE5MCPModule::HandleInitialize(const TSharedPtr<FJsonObject>& /*Params*/)
{
	TSharedPtr<FJsonObject> ToolsCap = MakeShared<FJsonObject>();
	ToolsCap->SetBoolField(TEXT("listChanged"), false);

	TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);

	TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), MCPConstants::ServerName);
	ServerInfo->SetStringField(TEXT("version"), MCPConstants::ServerVersion);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), MCPConstants::ProtocolVersion);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	return FMCPMethodResult::Success(Result);
}

FMCPMethodResult FUE5MCPModule::HandleToolsList(const TSharedPtr<FJsonObject>& /*Params*/)
{
	TArray<TSharedPtr<FJsonValue>> ToolsArray;
	if (ToolRegistry.IsValid())
	{
		ToolsArray = ToolRegistry->BuildToolsListJson();
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), ToolsArray);

	return FMCPMethodResult::Success(Result);
}

FMCPMethodResult FUE5MCPModule::HandleToolsCall(const TSharedPtr<FJsonObject>& Params)
{
	// Extract tool name
	FString ToolName;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("name"), ToolName);
	}

	if (ToolName.IsEmpty())
	{
		return FMCPMethodResult::Error(-32602, TEXT("Invalid params: missing tool name"));
	}

	// Look up tool in registry
	const FMCPToolRegistration* ToolReg = ToolRegistry.IsValid() ? ToolRegistry->FindTool(ToolName) : nullptr;
	if (!ToolReg)
	{
		return FMCPMethodResult::Error(-32602,
			FString::Printf(TEXT("Invalid params: tool '%s' not found"), *ToolName));
	}

	// Extract arguments from params (default to empty object)
	TSharedPtr<FJsonObject> Arguments = MakeShared<FJsonObject>();
	{
		const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;
		if (Params->TryGetObjectField(TEXT("arguments"), ArgsPtr) && ArgsPtr)
		{
			Arguments = *ArgsPtr;
		}
	}

	// Execute the tool handler with timing
	double StartTime = FPlatformTime::Seconds();
	TSharedPtr<FJsonObject> Envelope = ToolReg->Execute(Arguments);
	double DurationMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	// Record metrics
	if (ToolMetrics.IsValid() && RuntimeState.IsValid())
	{
		FString SessionId = RuntimeState->GetSnapshot().SessionId.ToString();
		ToolMetrics->RecordCall(ToolName, SessionId, Arguments, Envelope, DurationMs);
	}

	// Determine if the envelope contains an error
	bool bIsError = Envelope.IsValid() && Envelope->HasField(TEXT("error"));

	// Serialize envelope to text
	FString EnvelopeText;
	{
		auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&EnvelopeText);
		FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);
	}

	// Wrap in MCP content format: { content: [{ type: "text", text: "..." }], isError: bool }
	TSharedPtr<FJsonObject> ContentItem = MakeShared<FJsonObject>();
	ContentItem->SetStringField(TEXT("type"), TEXT("text"));
	ContentItem->SetStringField(TEXT("text"), EnvelopeText);

	TArray<TSharedPtr<FJsonValue>> ContentArray;
	ContentArray.Add(MakeShared<FJsonValueObject>(ContentItem));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("content"), ContentArray);
	Result->SetBoolField(TEXT("isError"), bIsError);

	return FMCPMethodResult::Success(Result);
}

FMCPMethodResult FUE5MCPModule::HandleResourcesRead(const TSharedPtr<FJsonObject>& Params)
{
	FString Uri;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("uri"), Uri);
	}

	if (Uri.IsEmpty())
	{
		return FMCPMethodResult::Error(-32602, TEXT("Invalid params: missing uri"));
	}

	if (!ResourceStore.IsValid())
	{
		return FMCPMethodResult::Error(-32603, TEXT("Internal error: resource store not available"));
	}

	TSharedPtr<FJsonObject> Resource = ResourceStore->Retrieve(Uri);
	if (!Resource.IsValid())
	{
		return FMCPMethodResult::Error(-32602, TEXT("Resource expired or not found"));
	}

	// Serialize resource as text content
	FString ResourceText;
	{
		auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResourceText);
		FJsonSerializer::Serialize(Resource.ToSharedRef(), Writer);
	}

	TSharedPtr<FJsonObject> ContentItem = MakeShared<FJsonObject>();
	ContentItem->SetStringField(TEXT("type"), TEXT("text"));
	ContentItem->SetStringField(TEXT("text"), ResourceText);
	ContentItem->SetStringField(TEXT("uri"), Uri);
	ContentItem->SetStringField(TEXT("mimeType"), TEXT("application/json"));

	TArray<TSharedPtr<FJsonValue>> Contents;
	Contents.Add(MakeShared<FJsonValueObject>(ContentItem));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("contents"), Contents);

	return FMCPMethodResult::Success(Result);
}

// ============================================================================
// JSON-RPC Response Builders
// ============================================================================

FString FUE5MCPModule::MakeSuccessResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
{
	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Resp->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
	Resp->SetObjectField(TEXT("result"), Result.IsValid() ? Result : MakeShared<FJsonObject>());
	return JsonToString(Resp);
}

FString FUE5MCPModule::MakeErrorResponse(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
{
	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetNumberField(TEXT("code"), static_cast<double>(Code));
	ErrorObj->SetStringField(TEXT("message"), Message);

	TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
	Resp->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Resp->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
	Resp->SetObjectField(TEXT("error"), ErrorObj);
	return JsonToString(Resp);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUE5MCPModule, UE5MCP)
