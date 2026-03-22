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
    Box             UMETA(DisplayName = "Box"),
    Teleporter      UMETA(DisplayName = "Teleporter"),
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
    case EMoveDirection::Left:  return FIntPoint(1, 0);
    case EMoveDirection::Right: return FIntPoint(-1, 0);
    default:                    return FIntPoint(0, 0);
    }
}

// ============================================================
// Cell Type Descriptor — 集中描述每种地块类型的属性
// 新增地块时只需在此处添加一行描述，即可自动适配通行判定、序列化、编辑器擦除等逻辑
// ============================================================
struct FCellTypeDescriptor
{
    EGridCellType Type;
    const TCHAR*  TypeString;            // 序列化用字符串 (如 "Wall")
    bool          bPassable;             // 默认可通行性 (Door 有运行时覆盖)
    bool          bNeedsFloorUnderlay;   // 渲染时需要底层地板
    bool          bEraseReplacesWithFloor; // 编辑器擦除时替换为地板而非移除
};

namespace GridTypeUtils
{
    /** 获取指定类型的描述符，未找到返回 nullptr */
    const FCellTypeDescriptor* GetDescriptor(EGridCellType Type);

    /** 获取完整描述符表 */
    TConstArrayView<FCellTypeDescriptor> GetAllDescriptors();

    FString CellTypeToString(EGridCellType Type);
    EGridCellType StringToCellType(const FString& Str);

    /**
     * 返回指定地块类型所需的 UGridTileComponent 子类。
     * 如果该类型不需要特定 component（如 Floor、Wall），返回 nullptr。
     * 用于验证 TileStyleCatalog 中的 ActorClass 是否配置正确。
     */
    UClass* GetRequiredTileComponentClass(EGridCellType Type);
}
