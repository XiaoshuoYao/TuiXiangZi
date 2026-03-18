#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SokobanGameInstance.generated.h"

class USokobanSaveGame;

UENUM(BlueprintType)
enum class ELevelSourceType : uint8
{
    Preset,
    Custom,
    Editor
};

UCLASS()
class TUIXIANGZI_API USokobanGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    // ---- Level selection state (set before map travel) ----

    UPROPERTY(BlueprintReadWrite, Category = "LevelSelect")
    FString SelectedLevelPath;

    UPROPERTY(BlueprintReadWrite, Category = "LevelSelect")
    ELevelSourceType SelectedLevelSource = ELevelSourceType::Preset;

    UPROPERTY(BlueprintReadWrite, Category = "LevelSelect")
    int32 SelectedPresetIndex = 0;

    // ---- Progression ----

    UFUNCTION(BlueprintCallable, Category = "Progression")
    void LoadProgress();

    UFUNCTION(BlueprintCallable, Category = "Progression")
    void SaveProgress();

    UFUNCTION(BlueprintCallable, Category = "Progression")
    void MarkPresetLevelCompleted(const FString& LevelFileName);

    UFUNCTION(BlueprintCallable, Category = "Progression")
    bool IsPresetLevelUnlocked(int32 PresetIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Progression")
    int32 GetHighestUnlockedPresetIndex() const;

    UFUNCTION(BlueprintCallable, Category = "Progression")
    bool IsPresetLevelCompleted(const FString& LevelFileName) const;

    // ---- Level discovery ----

    UFUNCTION(BlueprintCallable, Category = "LevelSelect")
    TArray<FString> GetPresetLevelPaths() const;

    UFUNCTION(BlueprintCallable, Category = "LevelSelect")
    TArray<FString> GetCustomLevelPaths() const;

    UFUNCTION(BlueprintCallable, Category = "LevelSelect")
    static FString GetPresetLevelDirectory();

private:
    UPROPERTY()
    USokobanSaveGame* CurrentSave = nullptr;
};
