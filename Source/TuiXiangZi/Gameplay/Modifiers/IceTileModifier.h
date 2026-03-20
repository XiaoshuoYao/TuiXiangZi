#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Modifiers/TileModifierComponent.h"
#include "IceTileModifier.generated.h"

UCLASS(Blueprintable, ClassGroup = "GridModifier",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UIceTileModifier : public UTileModifierComponent
{
	GENERATED_BODY()

public:
	virtual bool ShouldContinueMovement(EMoveDirection Direction) const override { return true; }
};
