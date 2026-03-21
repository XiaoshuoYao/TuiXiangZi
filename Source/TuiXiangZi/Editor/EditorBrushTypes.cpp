#include "Editor/EditorBrushTypes.h"

// ============================================================
// Brush Descriptor Table
// 新增笔刷时，在此表中添加一行即可
// ============================================================
static const FBrushDescriptor GBrushDescriptors[] =
{
    //  Brush                       CellType                    DisplayName       StatusName  Shortcut  IconColor
    { EEditorBrush::Floor,         EGridCellType::Floor,        TEXT("地板"),     TEXT("Floor"),   TEXT("1"), FLinearColor(1.0f, 1.0f, 1.0f) },
    { EEditorBrush::Wall,          EGridCellType::Wall,         TEXT("墙壁"),     TEXT("Wall"),    TEXT("2"), FLinearColor(0.533f, 0.533f, 0.533f) },
    { EEditorBrush::Ice,           EGridCellType::Ice,          TEXT("冰面"),     TEXT("Ice"),     TEXT("3"), FLinearColor(0.4f, 0.8f, 1.0f) },
    { EEditorBrush::Goal,          EGridCellType::Goal,         TEXT("目标"),     TEXT("Goal"),    TEXT("4"), FLinearColor(1.0f, 0.267f, 0.267f) },
    { EEditorBrush::Door,          EGridCellType::Door,         TEXT("机关门"),   TEXT("Door"),    TEXT("5"), FLinearColor(0.667f, 0.4f, 1.0f) },
    { EEditorBrush::PressurePlate, EGridCellType::PressurePlate,TEXT("压力板"),   TEXT("Plate"),   TEXT("6"), FLinearColor(1.0f, 0.533f, 0.0f) },
    { EEditorBrush::BoxSpawn,      EGridCellType::Box,          TEXT("箱子生成"), TEXT("Box"),     TEXT("7"), FLinearColor(1.0f, 0.667f, 0.0f) },
    { EEditorBrush::PlayerStart,   EGridCellType::Empty,        TEXT("玩家起点"), TEXT("Start"),   TEXT("8"), FLinearColor(0.267f, 1.0f, 0.267f) },
    { EEditorBrush::Eraser,        EGridCellType::Empty,        TEXT("橡皮擦"),   TEXT("Eraser"),  TEXT("E"), FLinearColor(1.0f, 0.267f, 0.267f) },
};

namespace BrushUtils
{
    TConstArrayView<FBrushDescriptor> GetAllBrushDescriptors()
    {
        return TConstArrayView<FBrushDescriptor>(GBrushDescriptors, UE_ARRAY_COUNT(GBrushDescriptors));
    }

    const FBrushDescriptor* GetDescriptor(EEditorBrush Brush)
    {
        for (const FBrushDescriptor& Desc : GBrushDescriptors)
        {
            if (Desc.Brush == Brush) return &Desc;
        }
        return nullptr;
    }

    EGridCellType BrushToCellType(EEditorBrush Brush)
    {
        const FBrushDescriptor* Desc = GetDescriptor(Brush);
        return Desc ? Desc->CellType : EGridCellType::Empty;
    }
}
