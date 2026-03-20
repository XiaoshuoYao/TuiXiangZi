#include "Grid/GridTypes.h"

namespace GridTypeUtils
{
    static const TMap<EGridCellType, FString> TypeToStringMap = {
        { EGridCellType::Empty,         TEXT("Empty") },
        { EGridCellType::Floor,         TEXT("Floor") },
        { EGridCellType::Wall,          TEXT("Wall") },
        { EGridCellType::PressurePlate, TEXT("PressurePlate") },
        { EGridCellType::Ice,           TEXT("Ice") },
        { EGridCellType::Goal,          TEXT("Goal") },
        { EGridCellType::Door,          TEXT("Door") },
        { EGridCellType::Box,           TEXT("Box") },
    };

    FString CellTypeToString(EGridCellType Type)
    {
        if (const FString* Found = TypeToStringMap.Find(Type))
        {
            return *Found;
        }
        return TEXT("Empty");
    }

    EGridCellType StringToCellType(const FString& Str)
    {
        for (const auto& Pair : TypeToStringMap)
        {
            if (Pair.Value == Str) return Pair.Key;
        }
        UE_LOG(LogTemp, Warning, TEXT("GridTypeUtils::StringToCellType: Unknown type '%s', defaulting to Empty"), *Str);
        return EGridCellType::Empty;
    }
}
