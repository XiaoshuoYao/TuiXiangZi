// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Standardized error codes for MCP Edit operations.
 * These are business-level error codes returned inside the envelope's error object,
 * distinct from the JSON-RPC protocol error codes and from MCPToolExecution error codes.
 */
namespace MCPEditErrors
{
	inline const FString AssetNotFound              = TEXT("AssetNotFound");
	inline const FString BlueprintNotFound           = TEXT("BlueprintNotFound");
	inline const FString InvalidPackagePath          = TEXT("InvalidPackagePath");
	inline const FString InvalidAssetName            = TEXT("InvalidAssetName");
	inline const FString NameConflict                = TEXT("NameConflict");
	inline const FString UnsupportedAssetType        = TEXT("UnsupportedAssetType");
	inline const FString PropertyNotFound            = TEXT("PropertyNotFound");
	inline const FString PropertyNotEditable         = TEXT("PropertyNotEditable");
	inline const FString TypeMismatch                = TEXT("TypeMismatch");
	inline const FString InvalidObjectReference      = TEXT("InvalidObjectReference");
	inline const FString InvalidClassReference       = TEXT("InvalidClassReference");
	inline const FString CompileFailed               = TEXT("CompileFailed");
	inline const FString SaveFailed                  = TEXT("SaveFailed");
	inline const FString DeleteBlockedByReferences   = TEXT("DeleteBlockedByReferences");
	inline const FString PathOutsideGameContent      = TEXT("PathOutsideGameContent");
	inline const FString ComponentNotFound           = TEXT("ComponentNotFound");

	// DataTable / DataAsset specific errors
	inline const FString InvalidAssetType            = TEXT("InvalidAssetType");
	inline const FString SchemaNotAvailable          = TEXT("SchemaNotAvailable");
	inline const FString UnsupportedRowStruct        = TEXT("UnsupportedRowStruct");
	inline const FString UnsupportedDataAssetClass   = TEXT("UnsupportedDataAssetClass");
	inline const FString FieldMissing                = TEXT("FieldMissing");
	inline const FString InvalidEnumValue            = TEXT("InvalidEnumValue");
	inline const FString DuplicateKey                = TEXT("DuplicateKey");
	inline const FString KeyNotFound                 = TEXT("KeyNotFound");
	inline const FString RowNameConflict             = TEXT("RowNameConflict");
	inline const FString CollectionNotFound          = TEXT("CollectionNotFound");
}
