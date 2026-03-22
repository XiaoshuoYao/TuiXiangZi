#include "Grid/GridTypes.h"

// ============================================================
// Cell Type Descriptor Table
// 新增地块类型时，在此表中添加一行即可
// ============================================================
//                                Type                          TypeString          Passable  FloorUnderlay  EraseToFloor
static const FCellTypeDescriptor GCellTypeDescriptors[] =
{
    { EGridCellType::Empty,         TEXT("Empty"),          false,    false,         false },
    { EGridCellType::Floor,         TEXT("Floor"),          true,     false,         false },
    { EGridCellType::Wall,          TEXT("Wall"),           false,    true,          true  },
    { EGridCellType::PressurePlate, TEXT("PressurePlate"),  true,     true,          true  },
    { EGridCellType::Ice,           TEXT("Ice"),            true,     false,         false },
    { EGridCellType::Goal,          TEXT("Goal"),           true,     true,          true  },
    { EGridCellType::Door,          TEXT("Door"),           false,    true,          false }, // passability 由运行时 bDoorOpen 覆盖
    { EGridCellType::Box,           TEXT("Box"),            true,     true,          true  },
    { EGridCellType::Teleporter,    TEXT("Teleporter"),     true,     true,          true  },
};

namespace GridTypeUtils
{
    const FCellTypeDescriptor* GetDescriptor(EGridCellType Type)
    {
        for (const FCellTypeDescriptor& Desc : GCellTypeDescriptors)
        {
            if (Desc.Type == Type) return &Desc;
        }
        return nullptr;
    }

    TConstArrayView<FCellTypeDescriptor> GetAllDescriptors()
    {
        return TConstArrayView<FCellTypeDescriptor>(GCellTypeDescriptors, UE_ARRAY_COUNT(GCellTypeDescriptors));
    }

    FString CellTypeToString(EGridCellType Type)
    {
        if (const FCellTypeDescriptor* Desc = GetDescriptor(Type))
        {
            return Desc->TypeString;
        }
        return TEXT("Empty");
    }

    EGridCellType StringToCellType(const FString& Str)
    {
        for (const FCellTypeDescriptor& Desc : GCellTypeDescriptors)
        {
            if (Str == Desc.TypeString) return Desc.Type;
        }
        UE_LOG(LogTemp, Warning, TEXT("GridTypeUtils::StringToCellType: Unknown type '%s', defaulting to Empty"), *Str);
        return EGridCellType::Empty;
    }
}
