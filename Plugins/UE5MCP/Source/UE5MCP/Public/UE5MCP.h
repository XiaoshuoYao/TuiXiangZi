// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FJsonObject;
class FJsonValue;
class FMCPToolRegistry;
struct FMCPRuntimeState;
class FMCPResourceStore;
class FMCPLogBuffer;
class FMCPSnapshotStore;
class FMCPToolMetrics;

/**
 * Result from a JSON-RPC method handler.
 * Either carries a success result or a JSON-RPC error.
 * Phase 3+ tool-level (business) errors use Envelope.error inside a success result.
 */
struct FMCPMethodResult
{
	TSharedPtr<FJsonObject> Result;    // Non-null on success
	int32 ErrorCode = 0;              // Non-zero on JSON-RPC protocol error
	FString ErrorMessage;

	bool IsError() const { return ErrorCode != 0; }

	static FMCPMethodResult Success(const TSharedPtr<FJsonObject>& InResult)
	{
		FMCPMethodResult R;
		R.Result = InResult;
		return R;
	}

	static FMCPMethodResult Error(int32 InCode, const FString& InMessage)
	{
		FMCPMethodResult R;
		R.ErrorCode = InCode;
		R.ErrorMessage = InMessage;
		return R;
	}
};

/**
 * UE5MCP Module - Provides a local MCP (Model Context Protocol) server inside UE Editor.
 *
 * Phase 1: HTTP server on 127.0.0.1 with /mcp route
 * Phase 2: JSON-RPC 2.0 dispatch (initialize, tools/list, tools/call)
 * Phase 3+: Tool registry, real tool implementations, envelope
 */
class FUE5MCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// ---- HTTP Server Lifecycle ----
	void StartHttpServer();
	void StopHttpServer();

	// ---- JSON-RPC Layer ----
	FString ProcessJsonRpcRequest(const FString& RequestBody);

	// ---- Method Dispatch (extensible via TMap) ----
	using FMethodHandler = TFunction<FMCPMethodResult(const TSharedPtr<FJsonObject>& Params)>;
	TMap<FString, FMethodHandler> MethodHandlers;
	void RegisterMethodHandlers();

	// ---- MCP Method Implementations ----
	FMCPMethodResult HandleInitialize(const TSharedPtr<FJsonObject>& Params);
	FMCPMethodResult HandleToolsList(const TSharedPtr<FJsonObject>& Params);
	FMCPMethodResult HandleToolsCall(const TSharedPtr<FJsonObject>& Params);

	// ---- JSON-RPC Response Builders ----
	static FString MakeSuccessResponse(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result);
	static FString MakeErrorResponse(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message);

	// ---- HTTP State (PIMPL - implementation hidden in .cpp) ----
	struct FHttpState;
	TUniquePtr<FHttpState> HttpState;

	// ---- Tool Registry (Phase 3+) ----
	TUniquePtr<FMCPToolRegistry> ToolRegistry;
	void InitializeToolRegistry();

	// ---- Runtime Services ----
	TUniquePtr<FMCPRuntimeState> RuntimeState;
	TUniquePtr<FMCPResourceStore> ResourceStore;
	TUniquePtr<FMCPLogBuffer> LogBuffer;
	TUniquePtr<FMCPSnapshotStore> SnapshotStore;
	TUniquePtr<FMCPToolMetrics> ToolMetrics;

	// ---- resources/read handler ----
	FMCPMethodResult HandleResourcesRead(const TSharedPtr<FJsonObject>& Params);

	// ---- Configuration ----
	static constexpr uint32 DefaultPort = 8765;
	static constexpr uint32 MaxPortRetries = 10;
};
