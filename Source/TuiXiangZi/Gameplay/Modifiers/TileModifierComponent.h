#pragma once

#include "CoreMinimal.h"
#include "Gameplay/GridTileComponent.h"
#include "Grid/GridTypes.h"
#include "TileModifierComponent.generated.h"

UCLASS(Abstract, Blueprintable, ClassGroup = "GridModifier",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UTileModifierComponent : public UGridTileComponent
{
	GENERATED_BODY()

public:
	/** Whether an actor on this tile should continue sliding in the given direction. */
	virtual bool ShouldContinueMovement(EMoveDirection Direction) const { return false; }
};
