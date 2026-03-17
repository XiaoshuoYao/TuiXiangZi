#pragma once

#include "CoreMinimal.h"
#include "GridTypes.generated.h"

UENUM(BlueprintType)
enum class EGridCellType : uint8
{
    Empty           UMETA(DisplayName = "Empty"),
    Floor           UMETA(DisplayName = "Floor"),
    Wall            UMETA(DisplayName = "Wall"),
    PressurePlate   UMETA(DisplayName = "PressurePlate"),
    Ice             UMETA(DisplayName = "Ice"),
    Goal            UMETA(DisplayName = "Goal"),
    Door            UMETA(DisplayName = "Door"),
};

UENUM(BlueprintType)
enum class EMoveDirection : uint8
{
    Up      UMETA(DisplayName = "Up"),
    Down    UMETA(DisplayName = "Down"),
    Left    UMETA(DisplayName = "Left"),
    Right   UMETA(DisplayName = "Right"),
};

USTRUCT(BlueprintType)
struct FGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    EGridCellType CellType = EGridCellType::Empty;

    // 使用 TWeakObjectPtr 避免阻止 GC，不加 UPROPERTY（运行时动态引用）
    TWeakObjectPtr<AActor> OccupyingActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    bool bDoorOpen = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GroupId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 ExtraParam = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    FName VisualStyleId = NAME_None;
};

// 坐标系约定：X 向右 (Right)，Y 向上 (Up)
FORCEINLINE FIntPoint DirectionToOffset(EMoveDirection Dir)
{
    switch (Dir)
    {
    case EMoveDirection::Up:    return FIntPoint(0, 1);
    case EMoveDirection::Down:  return FIntPoint(0, -1);
    case EMoveDirection::Left:  return FIntPoint(-1, 0);
    case EMoveDirection::Right: return FIntPoint(1, 0);
    default:                    return FIntPoint(0, 0);
    }
}

namespace GridTypeUtils
{
    FString CellTypeToString(EGridCellType Type);
    EGridCellType StringToCellType(const FString& Str);
}
