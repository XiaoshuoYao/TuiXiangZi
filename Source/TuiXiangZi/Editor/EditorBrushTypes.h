#pragma once

#include "CoreMinimal.h"
#include "EditorBrushTypes.generated.h"

UENUM(BlueprintType)
enum class EEditorBrush : uint8
{
    Floor,
    Wall,
    Ice,
    Goal,
    Door,
    PressurePlate,
    BoxSpawn,
    PlayerStart,
    Eraser
};

UENUM(BlueprintType)
enum class EEditorMode : uint8
{
    Normal,
    PlacingPlatesForDoor,
    EditingDoorGroup
};
