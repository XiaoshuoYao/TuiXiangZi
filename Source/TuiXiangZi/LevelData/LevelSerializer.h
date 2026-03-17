#pragma once

#include "CoreMinimal.h"
#include "LevelData/LevelDataTypes.h"
#include "LevelSerializer.generated.h"

UCLASS()
class TUIXIANGZI_API ULevelSerializer : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static bool SaveToJson(const FLevelData& Data, const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static bool LoadFromJson(const FString& FilePath, FLevelData& OutData);

    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static FString GetDefaultLevelDirectory();

    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static void GetAvailableLevelFiles(TArray<FString>& OutFileNames);

private:
    static TSharedRef<FJsonValue> IntPointToJsonValue(FIntPoint Point);
    static bool JsonValueToIntPoint(const TSharedPtr<FJsonValue>& Value, FIntPoint& OutPoint);
    static TSharedRef<FJsonValue> ColorToJsonValue(FLinearColor Color);
    static bool JsonValueToColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor);
    static TSharedRef<FJsonObject> CellDataToJson(const FCellData& Cell);
    static bool JsonToCellData(const TSharedPtr<FJsonObject>& JsonObj, FCellData& OutCell);
    static TSharedRef<FJsonObject> GroupStyleToJson(const FMechanismGroupStyleData& Style);
    static bool JsonToGroupStyle(const TSharedPtr<FJsonObject>& JsonObj, FMechanismGroupStyleData& OutStyle);
};
