#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Framework/SokobanGameInstance.h"
#include "LevelSelectEntryData.generated.h"

UCLASS(BlueprintType)
class TUIXIANGZI_API ULevelSelectEntryData : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    FString LevelName;

    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    FString LevelFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    int32 PresetIndex = -1;

    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    bool bIsUnlocked = true;

    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    bool bIsCompleted = false;

    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    ELevelSourceType SourceType = ELevelSourceType::Preset;

    UPROPERTY(BlueprintReadOnly, Category = "LevelSelect")
    bool bIsSelected = false;
};
