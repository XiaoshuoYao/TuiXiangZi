#pragma once

#include "CoreMinimal.h"
#include "GameEventPayload.generated.h"

USTRUCT(BlueprintType)
struct FGameEventPayload
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;
	FIntPoint GridPos = FIntPoint::ZeroValue;
	FIntPoint FromPos = FIntPoint::ZeroValue;
	FIntPoint ToPos   = FIntPoint::ZeroValue;
	int32 IntParam    = 0;
	float FloatParam  = 0.0f;
	bool BoolParam    = false;
	FName NameParam   = NAME_None;

	static FGameEventPayload MakeActorMoved(AActor* InActor, FIntPoint From, FIntPoint To)
	{
		FGameEventPayload P;
		P.Actor = InActor; P.FromPos = From; P.ToPos = To;
		return P;
	}
	static FGameEventPayload MakeGridPos(FIntPoint Pos)
	{
		FGameEventPayload P;
		P.GridPos = Pos;
		return P;
	}
	static FGameEventPayload MakeActorAtPos(AActor* InActor, FIntPoint Pos)
	{
		FGameEventPayload P;
		P.Actor = InActor; P.GridPos = Pos;
		return P;
	}
	static FGameEventPayload MakeInt(int32 Value)
	{
		FGameEventPayload P;
		P.IntParam = Value;
		return P;
	}
	static FGameEventPayload MakeGridBounds(FIntPoint Min, FIntPoint Max, float CellSize)
	{
		FGameEventPayload P;
		P.FromPos = Min;
		P.ToPos = Max;
		P.FloatParam = CellSize;
		return P;
	}
};
