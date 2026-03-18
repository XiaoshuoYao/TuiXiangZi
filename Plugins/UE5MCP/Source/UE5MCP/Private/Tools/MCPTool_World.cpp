// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_World.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Reflection/MCPReflectionCore.h"
#include "MCPToolRegistry.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Selection.h"
#endif

// ============================================================================
// Helpers
// ============================================================================
namespace MCPToolWorldPrivate
{
	/** Resolve a world parameter to a UWorld*. Supports type strings and world names. */
	UWorld* ResolveWorld(const FString& WorldParam)
	{
		if (!GEngine) return nullptr;

		const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();

		// Map type string to EWorldType
		auto MapType = [](const FString& TypeStr) -> TOptional<EWorldType::Type>
		{
			if (TypeStr.Equals(TEXT("Editor"), ESearchCase::IgnoreCase)) return EWorldType::Editor;
			if (TypeStr.Equals(TEXT("PIE"), ESearchCase::IgnoreCase)) return EWorldType::PIE;
			if (TypeStr.Equals(TEXT("Game"), ESearchCase::IgnoreCase)) return EWorldType::Game;
			return {};
		};

		TOptional<EWorldType::Type> TypeOpt = MapType(WorldParam);

		for (const FWorldContext& Ctx : Contexts)
		{
			if (!Ctx.World()) continue;

			// Match by type
			if (TypeOpt.IsSet() && Ctx.WorldType == TypeOpt.GetValue())
			{
				return Ctx.World();
			}

			// Match by world name
			if (Ctx.World()->GetMapName() == WorldParam)
			{
				return Ctx.World();
			}
		}

		return nullptr;
	}

	FString WorldTypeToString(EWorldType::Type Type)
	{
		switch (Type)
		{
		case EWorldType::Editor: return TEXT("Editor");
		case EWorldType::PIE: return TEXT("PIE");
		case EWorldType::Game: return TEXT("Game");
		default: return TEXT("Other");
		}
	}

	/** Resolve an object_id string to a UObject*. */
	UObject* ResolveObjectId(const FString& ObjectId)
	{
		if (ObjectId.IsEmpty()) return nullptr;

		// Strategy 1: Full object path
		UObject* Obj = StaticFindObject(UObject::StaticClass(), nullptr, *ObjectId);
		if (Obj) return Obj;

		// Strategy 2: Package path + object name (LoadObject)
		Obj = LoadObject<UObject>(nullptr, *ObjectId);
		if (Obj) return Obj;

		return nullptr;
	}
}
using namespace MCPToolWorldPrivate;

// ============================================================================
// ue_object_inspect
// ============================================================================
namespace ObjectInspect
{
	static constexpr const TCHAR* ToolName = TEXT("ue_object_inspect");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Inspect a UObject by ID: returns class, properties, and optionally functions.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_object_inspect");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString ObjectId = MCPToolExecution::GetStringParam(Args, TEXT("object_id"));
		int32 Depth = MCPToolExecution::GetIntParam(Args, TEXT("depth"), 1);
		bool bIncludeValues = MCPToolExecution::GetBoolParam(Args, TEXT("include_values"), true);
		bool bIncludeFunctions = MCPToolExecution::GetBoolParam(Args, TEXT("include_functions"), false);
		TArray<FString> PropertyFilter = MCPToolExecution::GetStringArrayParam(Args, TEXT("property_filter"));
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), false);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L1.reflection.object_inspect") }, {}, true);

		if (ObjectId.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: object_id"));
			return Envelope;
		}
		if (Depth < 0 || Depth > 10)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("depth must be between 0 and 10"));
			return Envelope;
		}

		UObject* Obj = ResolveObjectId(ObjectId);
		if (!Obj)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("Object not found: %s"), *ObjectId));
			return Envelope;
		}

		TSharedPtr<FJsonObject> ObjResult = MakeShared<FJsonObject>();
		ObjResult->SetStringField(TEXT("class"), Obj->GetClass()->GetPathName());
		ObjResult->SetStringField(TEXT("object_id"), Obj->GetPathName());

		// Properties
		TArray<TSharedPtr<FJsonValue>> PropertiesArray;
		if (Depth > 0)
		{
			TSet<FString> FilterSet;
			for (const FString& F : PropertyFilter) FilterSet.Add(F);

			for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
			{
				FProperty* Prop = *It;
				if (FilterSet.Num() > 0 && !FilterSet.Contains(Prop->GetName()))
				{
					continue;
				}

				TSharedPtr<FJsonObject> PropDesc = MCPReflection::BuildPropertyDesc(Prop, false);
				if (PropDesc.IsValid() && bIncludeValues)
				{
					// Add current value as string
					FString ValueStr;
					const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);
					Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Obj, PPF_None);
					PropDesc->SetStringField(TEXT("value"), ValueStr);
				}
				if (PropDesc.IsValid())
				{
					PropertiesArray.Add(MakeShared<FJsonValueObject>(PropDesc));
				}
			}
		}
		ObjResult->SetArrayField(TEXT("properties"), PropertiesArray);

		// Functions
		TArray<TSharedPtr<FJsonValue>> FunctionsArray;
		if (bIncludeFunctions)
		{
			for (TFieldIterator<UFunction> It(Obj->GetClass()); It; ++It)
			{
				UFunction* Func = *It;
				if (!(Func->FunctionFlags & FUNC_BlueprintCallable)) continue;

				TSharedPtr<FJsonObject> FuncDesc = MCPReflection::BuildFunctionDesc(Func);
				if (FuncDesc.IsValid())
				{
					FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncDesc));
				}
			}
		}
		ObjResult->SetArrayField(TEXT("functions"), FunctionsArray);

		Envelope->SetObjectField(TEXT("object"), ObjResult);

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_world_list
// ============================================================================
namespace WorldList
{
	static constexpr const TCHAR* ToolName = TEXT("ue_world_list");
	static constexpr const TCHAR* ToolDescription =
		TEXT("List all active worlds (Editor, PIE, Game, etc.) with their type and play state.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_world_list");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& /*Args*/,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.world.list") }, {}, true);

		TArray<TSharedPtr<FJsonValue>> WorldsArray;

		if (GEngine)
		{
			const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
			for (const FWorldContext& Ctx : Contexts)
			{
				UWorld* World = Ctx.World();
				if (!World) continue;

				TSharedPtr<FJsonObject> WorldObj = MakeShared<FJsonObject>();
				WorldObj->SetStringField(TEXT("name"), World->GetMapName());
				WorldObj->SetStringField(TEXT("type"), WorldTypeToString(Ctx.WorldType));
				WorldObj->SetBoolField(TEXT("is_playing"), World->HasBegunPlay());
				WorldsArray.Add(MakeShared<FJsonValueObject>(WorldObj));
			}
		}

		Envelope->SetArrayField(TEXT("worlds"), WorldsArray);
		return Envelope;
	}
}

// ============================================================================
// ue_actor_search
// ============================================================================
namespace ActorSearch
{
	static constexpr const TCHAR* ToolName = TEXT("ue_actor_search");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Search actors in a world by class, name substring, or tag. Supports pagination.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_actor_search");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString WorldParam = MCPToolExecution::GetStringParam(Args, TEXT("world"), TEXT("Editor"));
		FString ClassName = MCPToolExecution::GetStringParam(Args, TEXT("class"));
		FString NameContains = MCPToolExecution::GetStringParam(Args, TEXT("name_contains"));
		FString Tag = MCPToolExecution::GetStringParam(Args, TEXT("tag"));
		int32 Limit = MCPToolExecution::GetIntParam(Args, TEXT("limit"), 200);
		int32 Offset = MCPToolExecution::GetIntParam(Args, TEXT("offset"), 0);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.world.actor_search") }, {}, true);

		UWorld* World = ResolveWorld(WorldParam);
		if (!World)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("World not found: %s"), *WorldParam));
			Envelope->SetArrayField(TEXT("actors"), {});
			return Envelope;
		}

		// Resolve optional class filter
		UClass* FilterClass = nullptr;
		if (!ClassName.IsEmpty())
		{
			FilterClass = MCPReflection::ResolveClass(ClassName);
			if (!FilterClass)
			{
				MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
					FString::Printf(TEXT("Class not found: %s"), *ClassName));
				Envelope->SetArrayField(TEXT("actors"), {});
				return Envelope;
			}
		}

		// Collect matching actors
		struct FActorInfo
		{
			FString ObjectPath;
			FString Name;
			FString ClassName;
		};
		TArray<FActorInfo> MatchingActors;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			// Class filter
			if (FilterClass && !Actor->IsA(FilterClass)) continue;

			// Name filter (case-insensitive)
			if (!NameContains.IsEmpty() && !Actor->GetName().Contains(NameContains, ESearchCase::IgnoreCase)) continue;

			// Tag filter
			if (!Tag.IsEmpty() && !Actor->ActorHasTag(FName(*Tag))) continue;

			FActorInfo Info;
			Info.ObjectPath = Actor->GetPathName();
			Info.Name = Actor->GetName();
			Info.ClassName = Actor->GetClass()->GetPathName();
			MatchingActors.Add(MoveTemp(Info));
		}

		// Sort for stable output
		MatchingActors.Sort([](const FActorInfo& A, const FActorInfo& B)
		{
			return A.ObjectPath < B.ObjectPath;
		});

		// Paginate
		int32 Total = MatchingActors.Num();
		int32 EffectiveLimit = FMath::Min(Limit, Snap.MaxItems);
		int32 Start = FMath::Min(Offset, Total);
		int32 End = FMath::Min(Start + EffectiveLimit, Total);

		TArray<TSharedPtr<FJsonValue>> ActorsArray;
		for (int32 i = Start; i < End; ++i)
		{
			const FActorInfo& Info = MatchingActors[i];
			TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
			ActorObj->SetStringField(TEXT("object_path"), Info.ObjectPath);
			ActorObj->SetStringField(TEXT("name"), Info.Name);
			ActorObj->SetStringField(TEXT("class"), Info.ClassName);
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
		}

		Envelope->SetArrayField(TEXT("actors"), ActorsArray);
		Envelope->SetNumberField(TEXT("total"), Total);
		return Envelope;
	}
}

// ============================================================================
// ue_selection_get
// ============================================================================
namespace SelectionGet
{
	static constexpr const TCHAR* ToolName = TEXT("ue_selection_get");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Get the currently selected actors/objects in the editor.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_selection_get");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& /*Args*/,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L0.world.selection") }, {}, true);

#if WITH_EDITOR
		TArray<TSharedPtr<FJsonValue>> SelectedArray;

		if (GEditor)
		{
			USelection* SelectedActors = GEditor->GetSelectedActors();
			if (SelectedActors)
			{
				for (int32 i = 0; i < SelectedActors->Num(); ++i)
				{
					UObject* Obj = SelectedActors->GetSelectedObject(i);
					if (!Obj) continue;

					TSharedPtr<FJsonObject> SelObj = MakeShared<FJsonObject>();
					SelObj->SetStringField(TEXT("object_path"), Obj->GetPathName());
					SelObj->SetStringField(TEXT("class"), Obj->GetClass()->GetPathName());
					SelectedArray.Add(MakeShared<FJsonValueObject>(SelObj));
				}
			}

			// Also include non-actor selections
			USelection* SelectedObjects = GEditor->GetSelectedObjects();
			if (SelectedObjects)
			{
				for (int32 i = 0; i < SelectedObjects->Num(); ++i)
				{
					UObject* Obj = SelectedObjects->GetSelectedObject(i);
					if (!Obj) continue;

					TSharedPtr<FJsonObject> SelObj = MakeShared<FJsonObject>();
					SelObj->SetStringField(TEXT("object_path"), Obj->GetPathName());
					SelObj->SetStringField(TEXT("class"), Obj->GetClass()->GetPathName());
					SelectedArray.Add(MakeShared<FJsonValueObject>(SelObj));
				}
			}
		}

		Envelope->SetArrayField(TEXT("selected"), SelectedArray);
#else
		MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorUnsupported,
			TEXT("ue_selection_get is only available in Editor builds"));
		Envelope->SetArrayField(TEXT("selected"), {});
#endif

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_World::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
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

	RegisterGameThreadTool(ObjectInspect::ToolName, ObjectInspect::ToolDescription, ObjectInspect::SchemaDefName, ObjectInspect::ExecuteOnGameThread);
	RegisterGameThreadTool(WorldList::ToolName, WorldList::ToolDescription, WorldList::SchemaDefName, WorldList::ExecuteOnGameThread);
	RegisterGameThreadTool(ActorSearch::ToolName, ActorSearch::ToolDescription, ActorSearch::SchemaDefName, ActorSearch::ExecuteOnGameThread);
	RegisterGameThreadTool(SelectionGet::ToolName, SelectionGet::ToolDescription, SelectionGet::SchemaDefName, SelectionGet::ExecuteOnGameThread);
}
