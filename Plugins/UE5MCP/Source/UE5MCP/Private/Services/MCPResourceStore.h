// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

/**
 * In-memory store for large tool results.
 * When a result exceeds max_result_bytes or return_as_resource=true,
 * the full JSON is stored here and a URI reference is returned instead.
 */
class FMCPResourceStore
{
public:
	/** Store a JSON result, return its URI: ue5mcp://resources/{uuid} */
	FString Store(const TSharedPtr<FJsonObject>& Json)
	{
		FScopeLock Lock(&Mutex);
		PurgeExpired();

		FGuid Id = FGuid::NewGuid();
		FString Uri = FString::Printf(TEXT("ue5mcp://resources/%s"), *Id.ToString(EGuidFormats::DigitsWithHyphens));

		FStoredResource Entry;
		Entry.Json = Json;
		Entry.CreatedAt = FDateTime::UtcNow();
		Resources.Add(Uri, MoveTemp(Entry));

		return Uri;
	}

	/** Retrieve a stored resource by URI. Returns nullptr if not found or expired. */
	TSharedPtr<FJsonObject> Retrieve(const FString& Uri)
	{
		FScopeLock Lock(&Mutex);

		FStoredResource* Found = Resources.Find(Uri);
		if (!Found)
		{
			return nullptr;
		}

		if ((FDateTime::UtcNow() - Found->CreatedAt).GetTotalMinutes() > TTLMinutes)
		{
			Resources.Remove(Uri);
			return nullptr;
		}

		return Found->Json;
	}

	/**
	 * Check if a JSON result should be stored as a resource.
	 * If so, store it and return a resource envelope. Otherwise return the original.
	 */
	TSharedPtr<FJsonObject> MaybeStoreAsResource(
		const TSharedPtr<FJsonObject>& Envelope,
		int32 MaxResultBytes,
		bool bForceResource)
	{
		if (!bForceResource)
		{
			// Serialize to check size
			FString Serialized;
			auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Serialized);
			FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);

			if (Serialized.Len() * sizeof(TCHAR) <= static_cast<uint32>(MaxResultBytes))
			{
				return Envelope; // Small enough, return inline
			}
		}

		FString Uri = Store(Envelope);

		// Build resource reference envelope (preserve base envelope fields)
		TSharedPtr<FJsonObject> ResourceRef = MakeShared<FJsonObject>();
		// Copy envelope base fields
		if (Envelope->HasField(TEXT("schema_version")))
			ResourceRef->SetStringField(TEXT("schema_version"), Envelope->GetStringField(TEXT("schema_version")));
		if (Envelope->HasField(TEXT("capabilities")))
			ResourceRef->SetArrayField(TEXT("capabilities"), Envelope->GetArrayField(TEXT("capabilities")));
		if (Envelope->HasField(TEXT("warnings")))
			ResourceRef->SetArrayField(TEXT("warnings"), Envelope->GetArrayField(TEXT("warnings")));
		if (Envelope->HasField(TEXT("editor_only")))
			ResourceRef->SetBoolField(TEXT("editor_only"), Envelope->GetBoolField(TEXT("editor_only")));

		TSharedPtr<FJsonObject> ResourceObj = MakeShared<FJsonObject>();
		ResourceObj->SetStringField(TEXT("uri"), Uri);
		ResourceRef->SetObjectField(TEXT("resource"), ResourceObj);

		return ResourceRef;
	}

private:
	struct FStoredResource
	{
		TSharedPtr<FJsonObject> Json;
		FDateTime CreatedAt;
	};

	void PurgeExpired()
	{
		FDateTime Now = FDateTime::UtcNow();
		TArray<FString> ToRemove;
		for (const auto& Pair : Resources)
		{
			if ((Now - Pair.Value.CreatedAt).GetTotalMinutes() > TTLMinutes)
			{
				ToRemove.Add(Pair.Key);
			}
		}
		for (const FString& Key : ToRemove)
		{
			Resources.Remove(Key);
		}
	}

	mutable FCriticalSection Mutex;
	TMap<FString, FStoredResource> Resources;
	static constexpr double TTLMinutes = 5.0;
};
