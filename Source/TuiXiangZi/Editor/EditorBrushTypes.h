#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
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
    Teleporter,
    Eraser
};

UENUM(BlueprintType)
enum class EEditorMode : uint8
{
    Normal,
    PlacingPlatesForDoor,
    EditingDoorGroup,
    PlacingTeleporterPair
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

// ============================================================
// Brush Descriptor — 集中描述每种编辑器笔刷的属性
// 新增笔刷时在 EditorBrushTypes.cpp 的描述符表中添加一行即可
// ============================================================
struct FBrushDescriptor
{
    EEditorBrush  Brush;
    EGridCellType CellType;        // 对应的地块类型 (PlayerStart/Eraser 为 Empty)
    const TCHAR*  DisplayName;     // 中文显示名
    const TCHAR*  StatusName;      // 状态栏英文名
    const TCHAR*  Shortcut;        // 快捷键
    FLinearColor  IconColor;
};

namespace BrushUtils
{
    /** 获取完整笔刷描述符表 */
    TConstArrayView<FBrushDescriptor> GetAllBrushDescriptors();

    /** 查找指定笔刷的描述符 */
    const FBrushDescriptor* GetDescriptor(EEditorBrush Brush);

    /** 笔刷 → 地块类型映射 */
    EGridCellType BrushToCellType(EEditorBrush Brush);
}
