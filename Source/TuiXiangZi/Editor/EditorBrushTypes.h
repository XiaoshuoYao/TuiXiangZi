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

UENUM(BlueprintType)
enum class EValidationContext : uint8
{
    Save,
    Test
};

UENUM(BlueprintType)
enum class EToolbarAction : uint8
{
    New,
    Save,
    Load,
    Test,
    Back
};
