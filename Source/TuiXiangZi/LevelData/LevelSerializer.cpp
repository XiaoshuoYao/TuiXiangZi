#include "LevelData/LevelSerializer.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// -----------------------------------------------------------------------------
// IntPoint <-> JSON  ([x, y])
// -----------------------------------------------------------------------------

TSharedRef<FJsonValue> ULevelSerializer::IntPointToJsonValue(FIntPoint Point)
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Point.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Point.Y));
    return MakeShared<FJsonValueArray>(Arr);
}

bool ULevelSerializer::JsonValueToIntPoint(const TSharedPtr<FJsonValue>& Value, FIntPoint& OutPoint)
{
    if (!Value.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr;
    if (!Value->TryGetArray(Arr) || Arr->Num() < 2)
    {
        return false;
    }

    OutPoint.X = static_cast<int32>((*Arr)[0]->AsNumber());
    OutPoint.Y = static_cast<int32>((*Arr)[1]->AsNumber());
    return true;
}

// -----------------------------------------------------------------------------
// FLinearColor <-> JSON  ([r, g, b] or [r, g, b, a])
// -----------------------------------------------------------------------------

TSharedRef<FJsonValue> ULevelSerializer::ColorToJsonValue(FLinearColor Color)
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Color.R));
    Arr.Add(MakeShared<FJsonValueNumber>(Color.G));
    Arr.Add(MakeShared<FJsonValueNumber>(Color.B));

    if (!FMath::IsNearlyEqual(Color.A, 1.0f))
    {
        Arr.Add(MakeShared<FJsonValueNumber>(Color.A));
    }

    return MakeShared<FJsonValueArray>(Arr);
}

bool ULevelSerializer::JsonValueToColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor)
{
    if (!Value.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr;
    if (!Value->TryGetArray(Arr) || Arr->Num() < 3)
    {
        return false;
    }

    OutColor.R = static_cast<float>((*Arr)[0]->AsNumber());
    OutColor.G = static_cast<float>((*Arr)[1]->AsNumber());
    OutColor.B = static_cast<float>((*Arr)[2]->AsNumber());
    OutColor.A = (Arr->Num() >= 4) ? static_cast<float>((*Arr)[3]->AsNumber()) : 1.0f;
    return true;
}

// -----------------------------------------------------------------------------
// FCellData <-> JSON
// -----------------------------------------------------------------------------

TSharedRef<FJsonObject> ULevelSerializer::CellDataToJson(const FCellData& Cell)
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

    Obj->SetField(TEXT("gridPos"), IntPointToJsonValue(Cell.GridPos));
    Obj->SetStringField(TEXT("cellType"), Cell.CellType);

    // Omit optional fields when they carry default values
    if (Cell.VisualStyleId != NAME_None)
    {
        Obj->SetStringField(TEXT("visualStyleId"), Cell.VisualStyleId.ToString());
    }

    if (Cell.GroupId != -1)
    {
        Obj->SetNumberField(TEXT("groupId"), Cell.GroupId);
    }

    if (Cell.ExtraParam != 0)
    {
        Obj->SetNumberField(TEXT("extraParam"), Cell.ExtraParam);
    }

    return Obj;
}

bool ULevelSerializer::JsonToCellData(const TSharedPtr<FJsonObject>& JsonObj, FCellData& OutCell)
{
    if (!JsonObj.IsValid())
    {
        return false;
    }

    // GridPos (required)
    if (!JsonValueToIntPoint(JsonObj->TryGetField(TEXT("gridPos")), OutCell.GridPos))
    {
        return false;
    }

    // CellType (required)
    if (!JsonObj->TryGetStringField(TEXT("cellType"), OutCell.CellType))
    {
        return false;
    }

    // Optional fields
    FString StyleStr;
    if (JsonObj->TryGetStringField(TEXT("visualStyleId"), StyleStr))
    {
        OutCell.VisualStyleId = FName(*StyleStr);
    }
    else
    {
        OutCell.VisualStyleId = NAME_None;
    }

    if (!JsonObj->TryGetNumberField(TEXT("groupId"), OutCell.GroupId))
    {
        OutCell.GroupId = -1;
    }

    if (!JsonObj->TryGetNumberField(TEXT("extraParam"), OutCell.ExtraParam))
    {
        OutCell.ExtraParam = 0;
    }

    return true;
}

// -----------------------------------------------------------------------------
// FMechanismGroupStyleData <-> JSON
// -----------------------------------------------------------------------------

TSharedRef<FJsonObject> ULevelSerializer::GroupStyleToJson(const FMechanismGroupStyleData& Style)
{
    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();

    Obj->SetNumberField(TEXT("groupId"), Style.GroupId);
    Obj->SetStringField(TEXT("displayName"), Style.DisplayName);
    Obj->SetField(TEXT("baseColor"), ColorToJsonValue(Style.BaseColor));
    Obj->SetField(TEXT("activeColor"), ColorToJsonValue(Style.ActiveColor));

    return Obj;
}

bool ULevelSerializer::JsonToGroupStyle(const TSharedPtr<FJsonObject>& JsonObj, FMechanismGroupStyleData& OutStyle)
{
    if (!JsonObj.IsValid())
    {
        return false;
    }

    if (!JsonObj->TryGetNumberField(TEXT("groupId"), OutStyle.GroupId))
    {
        return false;
    }

    JsonObj->TryGetStringField(TEXT("displayName"), OutStyle.DisplayName);

    JsonValueToColor(JsonObj->TryGetField(TEXT("baseColor")), OutStyle.BaseColor);
    JsonValueToColor(JsonObj->TryGetField(TEXT("activeColor")), OutStyle.ActiveColor);

    return true;
}

// -----------------------------------------------------------------------------
// FBoxData <-> JSON
// -----------------------------------------------------------------------------

TSharedRef<FJsonValue> ULevelSerializer::BoxDataToJsonValue(const FBoxData& Box)
{
    // If no style, save as simple [x,y] for backward compat
    if (Box.VisualStyleId.IsNone())
    {
        return IntPointToJsonValue(Box.GridPos);
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetField(TEXT("pos"), IntPointToJsonValue(Box.GridPos));
    Obj->SetStringField(TEXT("visualStyleId"), Box.VisualStyleId.ToString());
    return MakeShared<FJsonValueObject>(Obj);
}

bool ULevelSerializer::JsonValueToBoxData(const TSharedPtr<FJsonValue>& Value, FBoxData& OutBox)
{
    if (!Value.IsValid()) return false;

    // New format: object with "pos" and "visualStyleId"
    const TSharedPtr<FJsonObject>* ObjPtr;
    if (Value->TryGetObject(ObjPtr))
    {
        if (!JsonValueToIntPoint((*ObjPtr)->TryGetField(TEXT("pos")), OutBox.GridPos))
            return false;
        FString StyleStr;
        if ((*ObjPtr)->TryGetStringField(TEXT("visualStyleId"), StyleStr))
            OutBox.VisualStyleId = FName(*StyleStr);
        else
            OutBox.VisualStyleId = NAME_None;
        return true;
    }

    // Old format: simple [x,y] array
    if (JsonValueToIntPoint(Value, OutBox.GridPos))
    {
        OutBox.VisualStyleId = NAME_None;
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// SaveToJson
// -----------------------------------------------------------------------------

bool ULevelSerializer::SaveToJson(const FLevelData& Data, const FString& FilePath)
{
    TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();

    // Cells
    TArray<TSharedPtr<FJsonValue>> CellsArray;
    for (const FCellData& Cell : Data.Cells)
    {
        CellsArray.Add(MakeShared<FJsonValueObject>(CellDataToJson(Cell)));
    }
    RootObj->SetArrayField(TEXT("cells"), CellsArray);

    // PlayerStart
    RootObj->SetField(TEXT("playerStart"), IntPointToJsonValue(Data.PlayerStart));

    // Boxes are now stored as cells (CellType=Box), no separate "boxes" field

    // GroupStyles
    TArray<TSharedPtr<FJsonValue>> GroupStylesArray;
    for (const FMechanismGroupStyleData& Style : Data.GroupStyles)
    {
        GroupStylesArray.Add(MakeShared<FJsonValueObject>(GroupStyleToJson(Style)));
    }
    RootObj->SetArrayField(TEXT("groupStyles"), GroupStylesArray);

    // Serialize to string
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (!FJsonSerializer::Serialize(RootObj, Writer))
    {
        return false;
    }

    // Ensure directory exists
    FString Directory = FPaths::GetPath(FilePath);
    IFileManager::Get().MakeDirectory(*Directory, true);

    // Write file as UTF-8 without BOM
    return FFileHelper::SaveStringToFile(OutputString, *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// -----------------------------------------------------------------------------
// LoadFromJson
// -----------------------------------------------------------------------------

bool ULevelSerializer::LoadFromJson(const FString& FilePath, FLevelData& OutData)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> RootObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        return false;
    }

    // Cells
    OutData.Cells.Empty();
    const TArray<TSharedPtr<FJsonValue>>* CellsArray;
    if (RootObj->TryGetArrayField(TEXT("cells"), CellsArray))
    {
        for (const TSharedPtr<FJsonValue>& CellValue : *CellsArray)
        {
            FCellData Cell;
            if (JsonToCellData(CellValue->AsObject(), Cell))
            {
                OutData.Cells.Add(Cell);
            }
        }
    }

    // PlayerStart
    JsonValueToIntPoint(RootObj->TryGetField(TEXT("playerStart")), OutData.PlayerStart);

    // Backward compat: convert old "boxes" array to Box cells
    const TArray<TSharedPtr<FJsonValue>>* BoxesArray;
    if (RootObj->TryGetArrayField(TEXT("boxes"), BoxesArray))
    {
        for (const TSharedPtr<FJsonValue>& BoxValue : *BoxesArray)
        {
            FBoxData Box;
            if (JsonValueToBoxData(BoxValue, Box))
            {
                FCellData BoxCell;
                BoxCell.GridPos = Box.GridPos;
                BoxCell.CellType = TEXT("Box");
                BoxCell.VisualStyleId = Box.VisualStyleId;
                OutData.Cells.Add(BoxCell);
            }
        }
    }

    // GroupStyles
    OutData.GroupStyles.Empty();
    const TArray<TSharedPtr<FJsonValue>>* GroupStylesArray;
    if (RootObj->TryGetArrayField(TEXT("groupStyles"), GroupStylesArray))
    {
        for (const TSharedPtr<FJsonValue>& StyleValue : *GroupStylesArray)
        {
            FMechanismGroupStyleData Style;
            if (JsonToGroupStyle(StyleValue->AsObject(), Style))
            {
                OutData.GroupStyles.Add(Style);
            }
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------

FString ULevelSerializer::GetDefaultLevelDirectory()
{
    return FPaths::ProjectSavedDir() / TEXT("Levels/");
}

void ULevelSerializer::GetAvailableLevelFiles(TArray<FString>& OutFileNames)
{
    OutFileNames.Empty();

    FString SearchPath = GetDefaultLevelDirectory() / TEXT("*.json");
    IFileManager::Get().FindFiles(OutFileNames, *SearchPath, true, false);
}
