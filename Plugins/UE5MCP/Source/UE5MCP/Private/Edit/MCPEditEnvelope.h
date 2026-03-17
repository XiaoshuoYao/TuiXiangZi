// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Envelope helpers specialized for Edit operations.
 * Extends the base MCPEnvelope pattern with dry-run, changes, and compile/save flags.
 */
namespace MCPEditEnvelope
{
	/**
	 * Create a unified return envelope for edit operations.
	 *
	 * @param bOk             Whether the operation succeeded.
	 * @param bDryRun         Whether this was a dry-run (preview only, no mutations).
	 * @param Changes         Array of change descriptors (each a JSON object).
	 * @param Warnings        Human-readable warning strings.
	 * @param bNeedsCompile   If true, the caller should compile after applying changes.
	 * @param bNeedsSave      If true, the caller should save the modified packages.
	 */
	TSharedPtr<FJsonObject> MakeEditEnvelope(
		bool bOk,
		bool bDryRun,
		const TArray<TSharedPtr<FJsonValue>>& Changes = {},
		const TArray<FString>& Warnings = {},
		bool bNeedsCompile = false,
		bool bNeedsSave = false);

	/**
	 * Set an error on an edit envelope.
	 * Sets "ok" to false and adds an "error" object with code and message.
	 *
	 * @param Envelope   The envelope to modify (must be non-null).
	 * @param ErrorCode  One of the MCPEditErrors constants.
	 * @param Message    Human-readable error description.
	 */
	void SetEditError(
		TSharedPtr<FJsonObject>& Envelope,
		const FString& ErrorCode,
		const FString& Message);
}
