// Copyright Epic Games, Inc. All Rights Reserved.

#include "Edit/MCPAssetPathValidator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

namespace MCPAssetPathValidator
{

FString ValidatePackagePath(const FString& PackagePath)
{
	if (PackagePath.IsEmpty())
	{
		return TEXT("Package path is empty");
	}

	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		return FString::Printf(TEXT("Package path must start with /Game/, got: %s"), *PackagePath);
	}

	if (PackagePath.EndsWith(TEXT("/")))
	{
		return FString::Printf(TEXT("Package path must not end with /, got: %s"), *PackagePath);
	}

	if (PackagePath.Contains(TEXT("..")))
	{
		return FString::Printf(TEXT("Package path must not contain '..', got: %s"), *PackagePath);
	}

	if (PackagePath.StartsWith(TEXT("/Engine/")))
	{
		return FString::Printf(TEXT("Package path under /Engine/ is not allowed: %s"), *PackagePath);
	}

	if (PackagePath.StartsWith(TEXT("/Plugins/")))
	{
		return FString::Printf(TEXT("Package path under /Plugins/ is not allowed: %s"), *PackagePath);
	}

	return FString();
}

FString ValidateAssetObjectPath(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return TEXT("Asset object path is empty");
	}

	// Split on the last dot to get package path and asset name
	int32 DotIndex = INDEX_NONE;
	if (!AssetPath.FindLastChar(TEXT('.'), DotIndex))
	{
		return FString::Printf(TEXT("Asset object path must contain '.' separator (e.g. /Game/Folder/Asset.Asset), got: %s"), *AssetPath);
	}

	FString PackagePath = AssetPath.Left(DotIndex);
	FString AssetName = AssetPath.Mid(DotIndex + 1);

	FString PathError = ValidatePackagePath(PackagePath);
	if (!PathError.IsEmpty())
	{
		return PathError;
	}

	if (AssetName.IsEmpty())
	{
		return FString::Printf(TEXT("Asset name portion is empty in object path: %s"), *AssetPath);
	}

	return FString();
}

FString ValidateAssetName(const FString& AssetName)
{
	if (AssetName.IsEmpty())
	{
		return TEXT("Asset name is empty");
	}

	if (AssetName.Len() > 256)
	{
		return FString::Printf(TEXT("Asset name exceeds 256 characters (length: %d)"), AssetName.Len());
	}

	for (int32 i = 0; i < AssetName.Len(); ++i)
	{
		TCHAR Ch = AssetName[i];
		bool bValid = FChar::IsAlpha(Ch) || FChar::IsDigit(Ch) || Ch == TEXT('_') || Ch == TEXT(' ');
		if (!bValid)
		{
			return FString::Printf(TEXT("Asset name contains invalid character '%c' at index %d. Only letters, digits, underscores, and spaces are allowed."), Ch, i);
		}
	}

	return FString();
}

bool DoesAssetExist(const FString& PackagePath, const FString& AssetName)
{
	check(IsInGameThread());

	FString FullPath = PackagePath / AssetName;
	FString ObjectPath = FullPath + TEXT(".") + AssetName;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FSoftObjectPath SoftPath(ObjectPath);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);

	return AssetData.IsValid();
}

FString NormalizeObjectPath(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return Path;
	}

	// Already contains a dot separator — assume it's already normalized
	if (Path.Contains(TEXT(".")))
	{
		return Path;
	}

	// Extract the last segment as the asset name
	int32 LastSlash = INDEX_NONE;
	if (Path.FindLastChar(TEXT('/'), LastSlash) && LastSlash < Path.Len() - 1)
	{
		FString AssetName = Path.Mid(LastSlash + 1);
		return Path + TEXT(".") + AssetName;
	}

	// Fallback: just append .Path
	return Path + TEXT(".") + Path;
}

} // namespace MCPAssetPathValidator
