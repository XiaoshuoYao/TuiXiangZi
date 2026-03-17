// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

/**
 * Snapshot metadata — lightweight info for listing without loading full data.
 */
struct FMCPSnapshotMeta
{
	FString Id;
	FDateTime CreatedAt;
	TArray<FString> ScopePaths;
	TArray<FString> ScopeClasses;
	TArray<FString> IncludeLevels;
	int32 AssetsCount = 0;
	int32 ClassesCount = 0;
	int32 BpsCount = 0;
	int32 MaterialsCount = 0;
	int64 SizeBytes = 0;
	double CollectTimeMs = 0;
	FString EngineVersion;
	FString ProjectName;

	TSharedPtr<FJsonObject> ToJson() const
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Id);
		Obj->SetStringField(TEXT("created_at"), CreatedAt.ToIso8601());

		TSharedPtr<FJsonObject> ScopeObj = MakeShared<FJsonObject>();
		{
			TArray<TSharedPtr<FJsonValue>> PathsArr;
			for (const FString& P : ScopePaths)
				PathsArr.Add(MakeShared<FJsonValueString>(P));
			ScopeObj->SetArrayField(TEXT("paths"), PathsArr);

			TArray<TSharedPtr<FJsonValue>> ClassesArr;
			for (const FString& C : ScopeClasses)
				ClassesArr.Add(MakeShared<FJsonValueString>(C));
			ScopeObj->SetArrayField(TEXT("classes"), ClassesArr);
		}
		Obj->SetObjectField(TEXT("scope"), ScopeObj);

		TArray<TSharedPtr<FJsonValue>> IncludeArr;
		for (const FString& L : IncludeLevels)
			IncludeArr.Add(MakeShared<FJsonValueString>(L));
		Obj->SetArrayField(TEXT("include"), IncludeArr);

		TSharedPtr<FJsonObject> StatsObj = MakeShared<FJsonObject>();
		StatsObj->SetNumberField(TEXT("assets_count"), AssetsCount);
		StatsObj->SetNumberField(TEXT("classes_count"), ClassesCount);
		StatsObj->SetNumberField(TEXT("bps_count"), BpsCount);
		StatsObj->SetNumberField(TEXT("materials_count"), MaterialsCount);
		StatsObj->SetNumberField(TEXT("size_bytes"), static_cast<double>(SizeBytes));
		StatsObj->SetNumberField(TEXT("collect_time_ms"), CollectTimeMs);
		Obj->SetObjectField(TEXT("stats"), StatsObj);

		Obj->SetStringField(TEXT("engine_version"), EngineVersion);
		Obj->SetStringField(TEXT("project_name"), ProjectName);

		return Obj;
	}

	static FMCPSnapshotMeta FromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FMCPSnapshotMeta Meta;
		if (!Obj.IsValid()) return Meta;

		Obj->TryGetStringField(TEXT("id"), Meta.Id);

		FString CreatedAtStr;
		if (Obj->TryGetStringField(TEXT("created_at"), CreatedAtStr))
		{
			FDateTime::ParseIso8601(*CreatedAtStr, Meta.CreatedAt);
		}

		const TSharedPtr<FJsonObject>* ScopePtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("scope"), ScopePtr) && ScopePtr)
		{
			const TArray<TSharedPtr<FJsonValue>>* PathsArr = nullptr;
			if ((*ScopePtr)->TryGetArrayField(TEXT("paths"), PathsArr))
			{
				for (const auto& V : *PathsArr)
				{
					FString S;
					if (V->TryGetString(S)) Meta.ScopePaths.Add(S);
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* ClassesArr = nullptr;
			if ((*ScopePtr)->TryGetArrayField(TEXT("classes"), ClassesArr))
			{
				for (const auto& V : *ClassesArr)
				{
					FString S;
					if (V->TryGetString(S)) Meta.ScopeClasses.Add(S);
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* IncludeArr = nullptr;
		if (Obj->TryGetArrayField(TEXT("include"), IncludeArr))
		{
			for (const auto& V : *IncludeArr)
			{
				FString S;
				if (V->TryGetString(S)) Meta.IncludeLevels.Add(S);
			}
		}

		const TSharedPtr<FJsonObject>* StatsPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("stats"), StatsPtr) && StatsPtr)
		{
			(*StatsPtr)->TryGetNumberField(TEXT("assets_count"), Meta.AssetsCount);
			(*StatsPtr)->TryGetNumberField(TEXT("classes_count"), Meta.ClassesCount);
			(*StatsPtr)->TryGetNumberField(TEXT("bps_count"), Meta.BpsCount);
			(*StatsPtr)->TryGetNumberField(TEXT("materials_count"), Meta.MaterialsCount);

			double SizeBytesD = 0;
			if ((*StatsPtr)->TryGetNumberField(TEXT("size_bytes"), SizeBytesD))
				Meta.SizeBytes = static_cast<int64>(SizeBytesD);

			(*StatsPtr)->TryGetNumberField(TEXT("collect_time_ms"), Meta.CollectTimeMs);
		}

		Obj->TryGetStringField(TEXT("engine_version"), Meta.EngineVersion);
		Obj->TryGetStringField(TEXT("project_name"), Meta.ProjectName);

		return Meta;
	}
};

/**
 * In-memory + disk store for project snapshots.
 * Supports LRU eviction for data cache (meta always cached).
 */
class FMCPSnapshotStore
{
public:
	/** Store a snapshot (both meta and data). Writes to memory cache and disk. */
	void Store(const FString& SnapshotId, const FMCPSnapshotMeta& Meta, const TSharedPtr<FJsonObject>& Data)
	{
		FScopeLock ScopeLock(&Lock);

		MetaCache.Add(SnapshotId, Meta);

		// LRU eviction for data cache
		if (DataCache.Num() >= MaxCachedData && !DataCache.Contains(SnapshotId))
		{
			EvictOldestData();
		}
		DataCache.Add(SnapshotId, Data);
		DataAccessOrder.Remove(SnapshotId);
		DataAccessOrder.Add(SnapshotId);

		// Write to disk
		FString SnapshotDir = GetSnapshotDir();
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*SnapshotDir);

		// Write meta
		{
			TSharedPtr<FJsonObject> MetaJson = Meta.ToJson();
			FString MetaStr;
			auto Writer = TJsonWriterFactory<>::Create(&MetaStr);
			FJsonSerializer::Serialize(MetaJson.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(MetaStr, *GetMetaPath(SnapshotId), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		// Write data
		{
			FString DataStr;
			auto Writer = TJsonWriterFactory<>::Create(&DataStr);
			FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);
			FFileHelper::SaveStringToFile(DataStr, *GetDataPath(SnapshotId), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
	}

	/** List all snapshot metadata, sorted by created_at descending. */
	TArray<FMCPSnapshotMeta> List() const
	{
		FScopeLock ScopeLock(&Lock);

		TArray<FMCPSnapshotMeta> Result;
		MetaCache.GenerateValueArray(Result);

		Result.Sort([](const FMCPSnapshotMeta& A, const FMCPSnapshotMeta& B)
		{
			return A.CreatedAt > B.CreatedAt;
		});

		return Result;
	}

	/** Get snapshot data by ID. Loads from disk if not in cache. */
	TSharedPtr<FJsonObject> Get(const FString& SnapshotId)
	{
		FScopeLock ScopeLock(&Lock);

		// Check memory cache
		TSharedPtr<FJsonObject>* Found = DataCache.Find(SnapshotId);
		if (Found)
		{
			// Update LRU
			DataAccessOrder.Remove(SnapshotId);
			DataAccessOrder.Add(SnapshotId);
			return *Found;
		}

		// Check if meta exists (snapshot is known)
		if (!MetaCache.Contains(SnapshotId))
		{
			return nullptr;
		}

		// Try loading from disk
		FString DataPath = GetDataPath(SnapshotId);
		FString DataStr;
		if (!FFileHelper::LoadFileToString(DataStr, *DataPath))
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> DataObj;
		auto Reader = TJsonReaderFactory<>::Create(DataStr);
		if (!FJsonSerializer::Deserialize(Reader, DataObj) || !DataObj.IsValid())
		{
			return nullptr;
		}

		// Cache it (with LRU eviction)
		if (DataCache.Num() >= MaxCachedData)
		{
			EvictOldestData();
		}
		DataCache.Add(SnapshotId, DataObj);
		DataAccessOrder.Add(SnapshotId);

		return DataObj;
	}

	/** Delete a snapshot from memory and disk. */
	bool Delete(const FString& SnapshotId)
	{
		FScopeLock ScopeLock(&Lock);

		MetaCache.Remove(SnapshotId);
		DataCache.Remove(SnapshotId);
		DataAccessOrder.Remove(SnapshotId);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.DeleteFile(*GetDataPath(SnapshotId));
		PlatformFile.DeleteFile(*GetMetaPath(SnapshotId));

		return true;
	}

	/** Scan disk for existing snapshot meta files and load them into MetaCache. */
	void LoadMetaFromDisk()
	{
		FScopeLock ScopeLock(&Lock);

		FString SnapshotDir = GetSnapshotDir();
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		if (!PlatformFile.DirectoryExists(*SnapshotDir))
		{
			return;
		}

		TArray<FString> MetaFiles;
		PlatformFile.FindFiles(MetaFiles, *SnapshotDir, TEXT(".meta.json"));

		for (const FString& MetaFile : MetaFiles)
		{
			FString MetaStr;
			if (!FFileHelper::LoadFileToString(MetaStr, *MetaFile))
			{
				continue;
			}

			TSharedPtr<FJsonObject> MetaJson;
			auto Reader = TJsonReaderFactory<>::Create(MetaStr);
			if (!FJsonSerializer::Deserialize(Reader, MetaJson) || !MetaJson.IsValid())
			{
				continue;
			}

			FMCPSnapshotMeta Meta = FMCPSnapshotMeta::FromJson(MetaJson);
			if (!Meta.Id.IsEmpty())
			{
				// Verify data file exists
				if (PlatformFile.FileExists(*GetDataPath(Meta.Id)))
				{
					MetaCache.Add(Meta.Id, Meta);
				}
			}
		}
	}

private:
	void EvictOldestData()
	{
		if (DataAccessOrder.Num() > 0)
		{
			FString Oldest = DataAccessOrder[0];
			DataAccessOrder.RemoveAt(0);
			DataCache.Remove(Oldest);
		}
	}

	FString GetSnapshotDir() const
	{
		return FPaths::ProjectSavedDir() / TEXT("UEMCP") / TEXT("Snapshots");
	}

	FString GetDataPath(const FString& Id) const
	{
		return GetSnapshotDir() / (Id + TEXT(".json"));
	}

	FString GetMetaPath(const FString& Id) const
	{
		return GetSnapshotDir() / (Id + TEXT(".meta.json"));
	}

	TMap<FString, FMCPSnapshotMeta> MetaCache;
	TMap<FString, TSharedPtr<FJsonObject>> DataCache;
	TArray<FString> DataAccessOrder; // LRU order (oldest first)

	mutable FCriticalSection Lock;
	static constexpr int32 MaxCachedData = 3;
};
