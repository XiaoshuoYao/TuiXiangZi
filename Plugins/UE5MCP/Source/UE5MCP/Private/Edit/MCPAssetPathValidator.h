// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Asset path validation utilities for MCP Edit tools.
 * All validation functions return an empty string on success,
 * or an error message describing the validation failure.
 */
namespace MCPAssetPathValidator
{
	/**
	 * Validate that a package path is legal and under /Game/.
	 * Rejects paths containing "..", ending with "/", or starting with /Engine/ or /Plugins/.
	 * @return Empty string on success, error message on failure.
	 */
	FString ValidatePackagePath(const FString& PackagePath);

	/**
	 * Validate an asset object path (e.g. /Game/Folder/Asset.Asset).
	 * Checks package path portion and verifies the dot-separated format.
	 * @return Empty string on success, error message on failure.
	 */
	FString ValidateAssetObjectPath(const FString& AssetPath);

	/**
	 * Validate an asset name for legality.
	 * Allows letters, digits, underscores, and spaces. Max length 256.
	 * @return Empty string on success, error message on failure.
	 */
	FString ValidateAssetName(const FString& AssetName);

	/**
	 * Check whether an asset already exists at the given package path with the given name.
	 * Must be called on the GameThread.
	 */
	bool DoesAssetExist(const FString& PackagePath, const FString& AssetName);

	/**
	 * Normalize an object path to the canonical Asset.Asset format.
	 * e.g. /Game/Folder/MyAsset -> /Game/Folder/MyAsset.MyAsset
	 * If the path already contains a dot, it is returned as-is.
	 */
	FString NormalizeObjectPath(const FString& Path);
}
