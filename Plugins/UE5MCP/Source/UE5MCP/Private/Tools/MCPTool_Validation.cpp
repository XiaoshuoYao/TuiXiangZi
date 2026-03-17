// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/MCPTool_Validation.h"
#include "Services/MCPToolExecution.h"
#include "Services/MCPRuntimeState.h"
#include "Services/MCPResourceStore.h"
#include "Reflection/MCPReflectionCore.h"
#include "MCPToolRegistry.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/PackageName.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

#if WITH_EDITOR
#include "KismetCompilerModule.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

// ============================================================================
// Shared Helpers
// ============================================================================
namespace
{
	/** Resolve a world parameter to a UWorld*. */
	UWorld* ResolveWorld(const FString& WorldParam)
	{
		if (!GEngine) return nullptr;

		const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();

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
			if (TypeOpt.IsSet() && Ctx.WorldType == TypeOpt.GetValue()) return Ctx.World();
			if (Ctx.World()->GetMapName() == WorldParam) return Ctx.World();
		}

		return nullptr;
	}

	/**
	 * Find broken hard dependencies for a single package.
	 * Returns array of JSON objects: { "source_asset": ..., "missing_target": ... }
	 */
	TArray<TSharedPtr<FJsonValue>> FindBrokenDependencies(
		IAssetRegistry& AssetRegistry,
		const FName& PackageName)
	{
		TArray<TSharedPtr<FJsonValue>> BrokenRefs;

		TArray<FName> DepNames;
		AssetRegistry.GetDependencies(PackageName, DepNames,
			UE::AssetRegistry::EDependencyCategory::Package);

		for (const FName& DepPkg : DepNames)
		{
			// Skip engine/script packages
			FString DepStr = DepPkg.ToString();
			if (DepStr.StartsWith(TEXT("/Script/")) || DepStr.StartsWith(TEXT("/Engine/")))
			{
				continue;
			}

			TArray<FAssetData> TargetAssets;
			AssetRegistry.GetAssetsByPackageName(DepPkg, TargetAssets);

			// Also check if the package path itself exists on disk
			bool bExists = TargetAssets.Num() > 0 || FPackageName::DoesPackageExist(DepStr);

			if (!bExists)
			{
				TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
				Item->SetStringField(TEXT("source_asset"), PackageName.ToString());
				Item->SetStringField(TEXT("missing_target"), DepStr);
				BrokenRefs.Add(MakeShared<FJsonValueObject>(Item));
			}
		}

		return BrokenRefs;
	}

	/** Default naming convention prefix map. */
	TMap<FString, FString> GetDefaultNamingPrefixes()
	{
		TMap<FString, FString> Prefixes;
		Prefixes.Add(TEXT("/Script/Engine.Blueprint"), TEXT("BP_"));
		Prefixes.Add(TEXT("/Script/Engine.Material"), TEXT("M_"));
		Prefixes.Add(TEXT("/Script/Engine.MaterialInstanceConstant"), TEXT("MI_"));
		Prefixes.Add(TEXT("/Script/Engine.Texture2D"), TEXT("T_"));
		Prefixes.Add(TEXT("/Script/Engine.StaticMesh"), TEXT("SM_"));
		Prefixes.Add(TEXT("/Script/Engine.SkeletalMesh"), TEXT("SK_"));
		Prefixes.Add(TEXT("/Script/UMGEditor.WidgetBlueprint"), TEXT("WBP_"));
		Prefixes.Add(TEXT("/Script/Engine.AnimBlueprint"), TEXT("ABP_"));
		Prefixes.Add(TEXT("/Script/Engine.SoundCue"), TEXT("SC_"));
		Prefixes.Add(TEXT("/Script/Engine.SoundWave"), TEXT("SW_"));
		Prefixes.Add(TEXT("/Script/Engine.ParticleSystem"), TEXT("PS_"));
		Prefixes.Add(TEXT("/Script/Engine.DataTable"), TEXT("DT_"));
		Prefixes.Add(TEXT("/Script/Engine.CurveFloat"), TEXT("CF_"));
		Prefixes.Add(TEXT("/Script/Engine.CurveLinearColor"), TEXT("CLC_"));
		return Prefixes;
	}
}

// ============================================================================
// ue_validate_references — broken hard-dependency check
// ============================================================================
namespace ValidateReferences
{
	static constexpr const TCHAR* ToolName = TEXT("ue_validate_references");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Check for broken hard dependencies (missing referenced packages) on an asset or folder.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_validate_references");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), false);

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L4.validation.references") }, {}, true);

		if (AssetPath.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing required parameter: asset_path"));
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		// Collect packages to check
		TArray<FName> PackagesToCheck;

		// Check if this is a directory path (ends with /)
		if (AssetPath.EndsWith(TEXT("/")))
		{
			FARFilter Filter;
			Filter.PackagePaths.Add(FName(*AssetPath));
			Filter.bRecursivePaths = true;

			TArray<FAssetData> Assets;
			AssetRegistry.GetAssets(Filter, Assets);

			TSet<FName> UniquePackages;
			for (const FAssetData& AD : Assets)
			{
				UniquePackages.Add(AD.PackageName);
			}
			PackagesToCheck = UniquePackages.Array();
		}
		else
		{
			// Single asset: convert to package name
			FName PkgName = FName(*FPackageName::ObjectPathToPackageName(AssetPath));
			PackagesToCheck.Add(PkgName);
		}

		// Check all packages
		TArray<TSharedPtr<FJsonValue>> AllBroken;
		for (const FName& Pkg : PackagesToCheck)
		{
			TArray<TSharedPtr<FJsonValue>> Broken = FindBrokenDependencies(AssetRegistry, Pkg);
			AllBroken.Append(Broken);
		}

		Envelope->SetArrayField(TEXT("broken_refs"), AllBroken);
		Envelope->SetNumberField(TEXT("total_checked"), PackagesToCheck.Num());
		Envelope->SetNumberField(TEXT("broken_count"), AllBroken.Num());

		TArray<FString> Warnings;
		Warnings.Add(TEXT("Only hard (package) dependencies are checked; soft references (TSoftObjectPtr) are not covered."));
		Envelope->SetArrayField(TEXT("warnings"), TArray<TSharedPtr<FJsonValue>>({ MakeShared<FJsonValueString>(Warnings[0]) }));

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_validate_naming — asset naming convention check
// ============================================================================
namespace ValidateNaming
{
	static constexpr const TCHAR* ToolName = TEXT("ue_validate_naming");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Check asset naming conventions (e.g. BP_, M_, T_ prefixes) for assets in a path or matching a pattern.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_validate_naming");

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& RuntimeState,
		FMCPResourceStore& ResourceStore)
	{
		FMCPRuntimeStateSnapshot Snap = RuntimeState.GetSnapshot();

		FString AssetPath = MCPToolExecution::GetStringParam(Args, TEXT("asset_path"));
		FString Folder = MCPToolExecution::GetStringParam(Args, TEXT("folder"));
		FString Pattern = MCPToolExecution::GetStringParam(Args, TEXT("pattern"));
		bool bReturnAsResource = MCPToolExecution::GetBoolParam(Args, TEXT("return_as_resource"), false);

		// Optional convention overrides: JSON object mapping class -> prefix
		// (handled via GetStringParam of individual entries if needed)

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L4.validation.naming") }, {}, true);

		// Need at least one filter
		FString SearchPath = !AssetPath.IsEmpty() ? AssetPath : Folder;
		if (SearchPath.IsEmpty() && Pattern.IsEmpty())
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("At least one of asset_path, folder, or pattern is required."));
			return Envelope;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		FARFilter Filter;
		if (!SearchPath.IsEmpty())
		{
			// If it doesn't end with /, treat as a specific asset -> use its parent folder
			if (!SearchPath.EndsWith(TEXT("/")))
			{
				FString PackagePath = FPackageName::ObjectPathToPackageName(SearchPath);
				// Get the parent path
				FString ParentPath;
				if (PackagePath.Split(TEXT("/"), &ParentPath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					Filter.PackagePaths.Add(FName(*ParentPath));
				}
			}
			else
			{
				Filter.PackagePaths.Add(FName(*SearchPath));
			}
			Filter.bRecursivePaths = true;
		}

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		// Text pattern filter
		if (!Pattern.IsEmpty())
		{
			Assets.RemoveAll([&Pattern](const FAssetData& Asset)
			{
				return !Asset.AssetName.ToString().Contains(Pattern);
			});
		}

		// Check naming convention
		TMap<FString, FString> Prefixes = GetDefaultNamingPrefixes();

		// Apply convention overrides from args if present
		const TSharedPtr<FJsonObject>* ConventionPtr = nullptr;
		if (Args->TryGetObjectField(TEXT("convention"), ConventionPtr) && ConventionPtr)
		{
			for (const auto& Pair : (*ConventionPtr)->Values)
			{
				FString PrefixVal;
				if (Pair.Value->TryGetString(PrefixVal))
				{
					Prefixes.Add(Pair.Key, PrefixVal);
				}
			}
		}

		TArray<TSharedPtr<FJsonValue>> Violations;
		int32 TotalChecked = 0;

		for (const FAssetData& AD : Assets)
		{
			FString ClassStr = AD.AssetClassPath.ToString();
			FString* ExpectedPrefix = Prefixes.Find(ClassStr);
			if (!ExpectedPrefix) continue;

			TotalChecked++;
			FString AssetName = AD.AssetName.ToString();

			if (!AssetName.StartsWith(*ExpectedPrefix))
			{
				TSharedPtr<FJsonObject> V = MakeShared<FJsonObject>();
				V->SetStringField(TEXT("asset"), AD.GetObjectPathString());
				V->SetStringField(TEXT("asset_class"), ClassStr);
				V->SetStringField(TEXT("expected_prefix"), *ExpectedPrefix);
				V->SetStringField(TEXT("actual_name"), AssetName);
				Violations.Add(MakeShared<FJsonValueObject>(V));
			}
		}

		Envelope->SetArrayField(TEXT("violations"), Violations);
		Envelope->SetNumberField(TEXT("total_checked"), TotalChecked);
		Envelope->SetNumberField(TEXT("violation_count"), Violations.Num());

		return ResourceStore.MaybeStoreAsResource(Envelope, Snap.MaxResultBytes, bReturnAsResource);
	}
}

// ============================================================================
// ue_validate_world_rules — scene rule validation
// ============================================================================
namespace ValidateWorldRules
{
	static constexpr const TCHAR* ToolName = TEXT("ue_validate_world_rules");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Validate scene rules (actor count limits, tag requirements, underground actors, hidden collision) against a world.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_validate_world_rules");

	using FRuleCheckFn = TFunction<TSharedPtr<FJsonObject>(UWorld*, const TSharedPtr<FJsonObject>&)>;

	// --- Rule: all_actors_have_tag ---
	TSharedPtr<FJsonObject> CheckAllActorsHaveTag(UWorld* World, const TSharedPtr<FJsonObject>& Params)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("rule"), TEXT("all_actors_have_tag"));

		FString ClassName = MCPToolExecution::GetStringParam(Params, TEXT("class"));
		FString Tag = MCPToolExecution::GetStringParam(Params, TEXT("tag"));

		if (ClassName.IsEmpty() || Tag.IsEmpty())
		{
			Result->SetBoolField(TEXT("pass"), false);
			Result->SetStringField(TEXT("reason"), TEXT("Missing required params: class, tag"));
			return Result;
		}

		UClass* FilterClass = MCPReflection::ResolveClass(ClassName);
		if (!FilterClass)
		{
			Result->SetBoolField(TEXT("pass"), false);
			Result->SetStringField(TEXT("reason"), FString::Printf(TEXT("Class not found: %s"), *ClassName));
			return Result;
		}

		TArray<TSharedPtr<FJsonValue>> ViolatingActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !Actor->IsA(FilterClass)) continue;
			if (!Actor->ActorHasTag(FName(*Tag)))
			{
				TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
				A->SetStringField(TEXT("name"), Actor->GetName());
				A->SetStringField(TEXT("path"), Actor->GetPathName());
				ViolatingActors.Add(MakeShared<FJsonValueObject>(A));
			}
		}

		Result->SetBoolField(TEXT("pass"), ViolatingActors.Num() == 0);
		Result->SetArrayField(TEXT("violating_actors"), ViolatingActors);
		return Result;
	}

	// --- Rule: max_actor_count ---
	TSharedPtr<FJsonObject> CheckMaxActorCount(UWorld* World, const TSharedPtr<FJsonObject>& Params)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("rule"), TEXT("max_actor_count"));

		FString ClassName = MCPToolExecution::GetStringParam(Params, TEXT("class"));
		int32 MaxCount = MCPToolExecution::GetIntParam(Params, TEXT("max"), 0);

		if (ClassName.IsEmpty() || MaxCount <= 0)
		{
			Result->SetBoolField(TEXT("pass"), false);
			Result->SetStringField(TEXT("reason"), TEXT("Missing required params: class, max (>0)"));
			return Result;
		}

		UClass* FilterClass = MCPReflection::ResolveClass(ClassName);
		if (!FilterClass)
		{
			Result->SetBoolField(TEXT("pass"), false);
			Result->SetStringField(TEXT("reason"), FString::Printf(TEXT("Class not found: %s"), *ClassName));
			return Result;
		}

		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (*It && (*It)->IsA(FilterClass)) Count++;
		}

		Result->SetBoolField(TEXT("pass"), Count <= MaxCount);
		Result->SetNumberField(TEXT("actual_count"), Count);
		Result->SetNumberField(TEXT("max_allowed"), MaxCount);
		return Result;
	}

	// --- Rule: no_underground_actors ---
	TSharedPtr<FJsonObject> CheckNoUndergroundActors(UWorld* World, const TSharedPtr<FJsonObject>& Params)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("rule"), TEXT("no_underground_actors"));

		// Default min_z: use world KillZ if available, otherwise -10000
		float MinZ = static_cast<float>(MCPToolExecution::GetIntParam(Params, TEXT("min_z"), 0));
		if (MinZ == 0.0f)
		{
			AWorldSettings* WS = World->GetWorldSettings();
			MinZ = WS ? WS->KillZ : -10000.f;
		}

		TArray<TSharedPtr<FJsonValue>> ViolatingActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			FVector Location = Actor->GetActorLocation();
			if (Location.Z < MinZ)
			{
				TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
				A->SetStringField(TEXT("name"), Actor->GetName());
				A->SetStringField(TEXT("path"), Actor->GetPathName());
				A->SetNumberField(TEXT("z"), Location.Z);
				ViolatingActors.Add(MakeShared<FJsonValueObject>(A));
			}
		}

		Result->SetBoolField(TEXT("pass"), ViolatingActors.Num() == 0);
		Result->SetNumberField(TEXT("min_z"), MinZ);
		Result->SetArrayField(TEXT("violating_actors"), ViolatingActors);
		return Result;
	}

	// --- Rule: no_hidden_with_collision ---
	TSharedPtr<FJsonObject> CheckNoHiddenWithCollision(UWorld* World, const TSharedPtr<FJsonObject>& /*Params*/)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("rule"), TEXT("no_hidden_with_collision"));

		TArray<TSharedPtr<FJsonValue>> ViolatingActors;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !Actor->IsHidden()) continue;

			// Check if any primitive component has collision enabled
			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

			for (UPrimitiveComponent* Comp : PrimitiveComponents)
			{
				if (Comp && Comp->IsCollisionEnabled())
				{
					TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
					A->SetStringField(TEXT("name"), Actor->GetName());
					A->SetStringField(TEXT("path"), Actor->GetPathName());
					A->SetStringField(TEXT("component"), Comp->GetName());
					ViolatingActors.Add(MakeShared<FJsonValueObject>(A));
					break; // One violation per actor is enough
				}
			}
		}

		Result->SetBoolField(TEXT("pass"), ViolatingActors.Num() == 0);
		Result->SetArrayField(TEXT("violating_actors"), ViolatingActors);
		return Result;
	}

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		FString WorldParam = MCPToolExecution::GetStringParam(Args, TEXT("world"), TEXT("Editor"));

		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L4.validation.world_rules") }, {}, true);

		UWorld* World = ResolveWorld(WorldParam);
		if (!World)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorNotFound,
				FString::Printf(TEXT("World not found: %s"), *WorldParam));
			return Envelope;
		}

		// Parse rules array
		const TArray<TSharedPtr<FJsonValue>>* RulesArray = nullptr;
		if (!Args->TryGetArrayField(TEXT("rules"), RulesArray) || !RulesArray || RulesArray->Num() == 0)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing or empty required parameter: rules"));
			return Envelope;
		}

		// Rule dispatch map
		TMap<FString, FRuleCheckFn> RuleDispatch;
		RuleDispatch.Add(TEXT("all_actors_have_tag"), CheckAllActorsHaveTag);
		RuleDispatch.Add(TEXT("max_actor_count"), CheckMaxActorCount);
		RuleDispatch.Add(TEXT("no_underground_actors"), CheckNoUndergroundActors);
		RuleDispatch.Add(TEXT("no_hidden_with_collision"), CheckNoHiddenWithCollision);

		TArray<TSharedPtr<FJsonValue>> Results;
		int32 PassCount = 0;
		int32 FailCount = 0;

		for (const TSharedPtr<FJsonValue>& RuleValue : *RulesArray)
		{
			const TSharedPtr<FJsonObject>* RuleObj = nullptr;
			if (!RuleValue->TryGetObject(RuleObj) || !RuleObj) continue;

			FString RuleType;
			(*RuleObj)->TryGetStringField(TEXT("type"), RuleType);

			FRuleCheckFn* CheckFn = RuleDispatch.Find(RuleType);
			if (!CheckFn)
			{
				TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
				R->SetStringField(TEXT("rule"), RuleType);
				R->SetBoolField(TEXT("pass"), false);
				R->SetStringField(TEXT("reason"), FString::Printf(TEXT("Unknown rule type: %s"), *RuleType));
				Results.Add(MakeShared<FJsonValueObject>(R));
				FailCount++;
				continue;
			}

			TSharedPtr<FJsonObject> RuleResult = (*CheckFn)(World, *RuleObj);
			if (RuleResult.IsValid())
			{
				bool bPass = false;
				RuleResult->TryGetBoolField(TEXT("pass"), bPass);
				if (bPass) PassCount++; else FailCount++;
				Results.Add(MakeShared<FJsonValueObject>(RuleResult));
			}
		}

		Envelope->SetArrayField(TEXT("results"), Results);
		Envelope->SetNumberField(TEXT("passed"), PassCount);
		Envelope->SetNumberField(TEXT("failed"), FailCount);
		Envelope->SetNumberField(TEXT("total"), PassCount + FailCount);

		return Envelope;
	}
}

// ============================================================================
// ue_evaluate_goal — goal assertion evaluation
// ============================================================================
namespace EvaluateGoal
{
	static constexpr const TCHAR* ToolName = TEXT("ue_evaluate_goal");
	static constexpr const TCHAR* ToolDescription =
		TEXT("Evaluate a set of assertions (asset exists, property equals, blueprint compiles, actor count, etc.) and return pass/fail/score.");
	static constexpr const TCHAR* SchemaDefName = TEXT("Tool_ue_evaluate_goal");

	using FAssertionFn = TFunction<bool(const TSharedPtr<FJsonObject>&, FString&)>;

	// --- Assertion: asset_exists ---
	bool AssertAssetExists(const TSharedPtr<FJsonObject>& Params, FString& OutReason)
	{
		FString Path = MCPToolExecution::GetStringParam(Params, TEXT("path"));
		if (Path.IsEmpty())
		{
			OutReason = TEXT("Missing param: path");
			return false;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FAssetData AD = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Path));
		if (!AD.IsValid())
		{
			TArray<FAssetData> ByPkg;
			AssetRegistry.GetAssetsByPackageName(FName(*Path), ByPkg);
			if (ByPkg.Num() > 0) AD = ByPkg[0];
		}

		if (!AD.IsValid())
		{
			OutReason = FString::Printf(TEXT("Asset not found: %s"), *Path);
			return false;
		}
		return true;
	}

	// --- Assertion: property_equals ---
	bool AssertPropertyEquals(const TSharedPtr<FJsonObject>& Params, FString& OutReason)
	{
		FString ActorPath = MCPToolExecution::GetStringParam(Params, TEXT("actor"));
		FString PropertyName = MCPToolExecution::GetStringParam(Params, TEXT("property"));
		FString Expected = MCPToolExecution::GetStringParam(Params, TEXT("expected"));

		if (ActorPath.IsEmpty() || PropertyName.IsEmpty())
		{
			OutReason = TEXT("Missing params: actor, property");
			return false;
		}

		UObject* Obj = StaticFindObject(UObject::StaticClass(), nullptr, *ActorPath);
		if (!Obj) Obj = LoadObject<UObject>(nullptr, *ActorPath);
		if (!Obj)
		{
			OutReason = FString::Printf(TEXT("Object not found: %s"), *ActorPath);
			return false;
		}

		FProperty* Prop = Obj->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Prop)
		{
			OutReason = FString::Printf(TEXT("Property not found: %s on %s"), *PropertyName, *ActorPath);
			return false;
		}

		FString ValueStr;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);
		Prop->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, Obj, PPF_None);

		if (ValueStr != Expected)
		{
			OutReason = FString::Printf(TEXT("Expected '%s' but got '%s'"), *Expected, *ValueStr);
			return false;
		}
		return true;
	}

	// --- Assertion: blueprint_compiles ---
	bool AssertBlueprintCompiles(const TSharedPtr<FJsonObject>& Params, FString& OutReason)
	{
#if WITH_EDITOR
		FString Path = MCPToolExecution::GetStringParam(Params, TEXT("path"));
		if (Path.IsEmpty())
		{
			OutReason = TEXT("Missing param: path");
			return false;
		}

		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path);
		if (!BP)
		{
			OutReason = FString::Printf(TEXT("Blueprint not found: %s"), *Path);
			return false;
		}

		FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave);

		if (BP->Status == BS_Error)
		{
			OutReason = TEXT("Blueprint has compile errors");
			return false;
		}
		return true;
#else
		OutReason = TEXT("blueprint_compiles is only available in Editor builds (skipped)");
		return true; // Return true (skip) in non-editor builds
#endif
	}

	// --- Assertion: actor_count ---
	bool AssertActorCount(const TSharedPtr<FJsonObject>& Params, FString& OutReason)
	{
		FString ClassName = MCPToolExecution::GetStringParam(Params, TEXT("class"));
		FString Op = MCPToolExecution::GetStringParam(Params, TEXT("op"), TEXT(">="));
		int32 Value = MCPToolExecution::GetIntParam(Params, TEXT("value"), 0);
		FString WorldParam = MCPToolExecution::GetStringParam(Params, TEXT("world"), TEXT("Editor"));

		if (ClassName.IsEmpty())
		{
			OutReason = TEXT("Missing param: class");
			return false;
		}

		UWorld* World = ResolveWorld(WorldParam);
		if (!World)
		{
			OutReason = FString::Printf(TEXT("World not found: %s"), *WorldParam);
			return false;
		}

		UClass* FilterClass = MCPReflection::ResolveClass(ClassName);
		if (!FilterClass)
		{
			OutReason = FString::Printf(TEXT("Class not found: %s"), *ClassName);
			return false;
		}

		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (*It && (*It)->IsA(FilterClass)) Count++;
		}

		bool bPass = false;
		if (Op == TEXT(">=")) bPass = Count >= Value;
		else if (Op == TEXT("<=")) bPass = Count <= Value;
		else if (Op == TEXT("==")) bPass = Count == Value;
		else if (Op == TEXT(">")) bPass = Count > Value;
		else if (Op == TEXT("<")) bPass = Count < Value;
		else if (Op == TEXT("!=")) bPass = Count != Value;
		else
		{
			OutReason = FString::Printf(TEXT("Unknown operator: %s"), *Op);
			return false;
		}

		if (!bPass)
		{
			OutReason = FString::Printf(TEXT("Actor count %d does not satisfy %s %d"), Count, *Op, Value);
		}
		return bPass;
	}

	// --- Assertion: no_broken_references ---
	bool AssertNoBrokenReferences(const TSharedPtr<FJsonObject>& Params, FString& OutReason)
	{
		FString Path = MCPToolExecution::GetStringParam(Params, TEXT("path"));
		if (Path.IsEmpty())
		{
			OutReason = TEXT("Missing param: path");
			return false;
		}

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		FName PkgName = FName(*FPackageName::ObjectPathToPackageName(Path));

		TArray<TSharedPtr<FJsonValue>> Broken = FindBrokenDependencies(AssetRegistry, PkgName);
		if (Broken.Num() > 0)
		{
			OutReason = FString::Printf(TEXT("%d broken reference(s) found"), Broken.Num());
			return false;
		}
		return true;
	}

	// --- Assertion: tag_exists_on ---
	bool AssertTagExistsOn(const TSharedPtr<FJsonObject>& Params, FString& OutReason)
	{
		FString ActorPath = MCPToolExecution::GetStringParam(Params, TEXT("actor"));
		FString Tag = MCPToolExecution::GetStringParam(Params, TEXT("tag"));

		if (ActorPath.IsEmpty() || Tag.IsEmpty())
		{
			OutReason = TEXT("Missing params: actor, tag");
			return false;
		}

		UObject* Obj = StaticFindObject(UObject::StaticClass(), nullptr, *ActorPath);
		if (!Obj) Obj = LoadObject<UObject>(nullptr, *ActorPath);

		AActor* Actor = Cast<AActor>(Obj);
		if (!Actor)
		{
			OutReason = FString::Printf(TEXT("Actor not found: %s"), *ActorPath);
			return false;
		}

		if (!Actor->ActorHasTag(FName(*Tag)))
		{
			OutReason = FString::Printf(TEXT("Actor '%s' does not have tag '%s'"), *Actor->GetName(), *Tag);
			return false;
		}
		return true;
	}

	TSharedPtr<FJsonObject> ExecuteOnGameThread(
		const TSharedPtr<FJsonObject>& Args,
		FMCPRuntimeState& /*RuntimeState*/,
		FMCPResourceStore& /*ResourceStore*/)
	{
		TSharedPtr<FJsonObject> Envelope = MCPEnvelope::MakeEnvelope(
			{ TEXT("L4.validation.evaluate_goal") }, {}, true);

		// Parse assertions array
		const TArray<TSharedPtr<FJsonValue>>* AssertionsArray = nullptr;
		if (!Args->TryGetArrayField(TEXT("assertions"), AssertionsArray) || !AssertionsArray || AssertionsArray->Num() == 0)
		{
			MCPEnvelope::SetEnvelopeError(Envelope, MCPToolExecution::ErrorInvalidArgument,
				TEXT("Missing or empty required parameter: assertions"));
			return Envelope;
		}

		// Assertion dispatch map
		TMap<FString, FAssertionFn> AssertionDispatch;
		AssertionDispatch.Add(TEXT("asset_exists"), AssertAssetExists);
		AssertionDispatch.Add(TEXT("property_equals"), AssertPropertyEquals);
		AssertionDispatch.Add(TEXT("blueprint_compiles"), AssertBlueprintCompiles);
		AssertionDispatch.Add(TEXT("actor_count"), AssertActorCount);
		AssertionDispatch.Add(TEXT("no_broken_references"), AssertNoBrokenReferences);
		AssertionDispatch.Add(TEXT("tag_exists_on"), AssertTagExistsOn);

		TArray<TSharedPtr<FJsonValue>> Results;
		int32 PassCount = 0;
		int32 FailCount = 0;

		for (const TSharedPtr<FJsonValue>& AssertValue : *AssertionsArray)
		{
			const TSharedPtr<FJsonObject>* AssertObj = nullptr;
			if (!AssertValue->TryGetObject(AssertObj) || !AssertObj) continue;

			FString AssertType;
			(*AssertObj)->TryGetStringField(TEXT("type"), AssertType);

			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("type"), AssertType);

			FAssertionFn* CheckFn = AssertionDispatch.Find(AssertType);
			if (!CheckFn)
			{
				R->SetBoolField(TEXT("pass"), false);
				R->SetStringField(TEXT("reason"), FString::Printf(TEXT("Unknown assertion type: %s"), *AssertType));
				FailCount++;
				Results.Add(MakeShared<FJsonValueObject>(R));
				continue;
			}

			FString Reason;
			bool bPass = (*CheckFn)(*AssertObj, Reason);

			R->SetBoolField(TEXT("pass"), bPass);
			if (!bPass && !Reason.IsEmpty())
			{
				R->SetStringField(TEXT("reason"), Reason);
			}

			if (bPass) PassCount++; else FailCount++;
			Results.Add(MakeShared<FJsonValueObject>(R));
		}

		int32 Total = PassCount + FailCount;
		double Score = Total > 0 ? static_cast<double>(PassCount) / Total : 0.0;

		Envelope->SetArrayField(TEXT("results"), Results);
		Envelope->SetNumberField(TEXT("passed"), PassCount);
		Envelope->SetNumberField(TEXT("failed"), FailCount);
		Envelope->SetNumberField(TEXT("total"), Total);
		Envelope->SetNumberField(TEXT("score"), Score);

		return Envelope;
	}
}

// ============================================================================
// Registration
// ============================================================================
void MCPTool_Validation::RegisterAll(FMCPToolRegistry& Registry, FMCPRuntimeState& RuntimeState, FMCPResourceStore& ResourceStore)
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

	RegisterGameThreadTool(ValidateReferences::ToolName, ValidateReferences::ToolDescription, ValidateReferences::SchemaDefName, ValidateReferences::ExecuteOnGameThread);
	RegisterGameThreadTool(ValidateNaming::ToolName, ValidateNaming::ToolDescription, ValidateNaming::SchemaDefName, ValidateNaming::ExecuteOnGameThread);
	RegisterGameThreadTool(ValidateWorldRules::ToolName, ValidateWorldRules::ToolDescription, ValidateWorldRules::SchemaDefName, ValidateWorldRules::ExecuteOnGameThread);
	RegisterGameThreadTool(EvaluateGoal::ToolName, EvaluateGoal::ToolDescription, EvaluateGoal::SchemaDefName, EvaluateGoal::ExecuteOnGameThread);
}
