#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Events/GameEventPayload.h"
#include "SokobanGameMode.generated.h"

class AGridManager;
class UGameEventBus;
class UPauseMenuWidget;
class UTutorialDataAsset;
class UTutorialWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStepCountChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelCompleted, int32, TotalSteps);

UCLASS()
class TUIXIANGZI_API ASokobanGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASokobanGameMode();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void LoadLevel(const FString& JsonFilePath);

    UFUNCTION(BlueprintCallable, Category = "Game")
    void LoadNextLevel();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ResetCurrentLevel();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void UndoLastMove();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ReturnToEditor();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ReturnToMainMenu();

    // ===== Pause Menu =====
    UFUNCTION(BlueprintCallable, Category = "Game|UI")
    void TogglePauseMenu();

    UFUNCTION(BlueprintCallable, Category = "Game|UI")
    void ShowPauseMenu();

    UFUNCTION(BlueprintCallable, Category = "Game|UI")
    void HidePauseMenu();

    UFUNCTION(BlueprintCallable, Category = "Game|UI")
    void ShowLevelCompleteMenu(int32 Steps);

    UFUNCTION(BlueprintCallable, Category = "Game|UI")
    bool IsPauseMenuVisible() const { return bPauseMenuVisible; }

    void OnPlayerEnteredGoalEvent(FName EventTag, const FGameEventPayload& Payload);

    // ===== HUD Data Interface =====
    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    int32 GetCurrentStepCount() const;

    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    FString GetCurrentLevelName() const;

    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    bool IsLevelCompleted() const;

    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    void RequestUndo();

    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    void RequestReset();

    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    void RequestNextLevel();

    UFUNCTION(BlueprintCallable, Category = "Game|HUD")
    void RequestReturnToMenu();

    // ===== Delegates =====
    UPROPERTY(BlueprintAssignable, Category = "Game")
    FOnStepCountChanged OnStepCountChanged;

    UPROPERTY(BlueprintAssignable, Category = "Game")
    FOnLevelCompleted OnLevelCompleted;

    /** Called after each move to update box-on-plate visuals. */
    void UpdateBoxOnPlateVisuals();

protected:
    virtual void BeginPlay() override;
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
    TArray<FString> LevelJsonPaths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    int32 CurrentLevelIndex = 0;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
    UTutorialDataAsset* TutorialData = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "Tutorial")
    TSubclassOf<UTutorialWidget> TutorialWidgetClass;

private:
    UPROPERTY()
    AGridManager* GridManagerRef = nullptr;

    UPROPERTY()
    UPauseMenuWidget* PauseMenuWidget = nullptr;

    UPROPERTY()
    bool bPauseMenuVisible = false;

    FString CurrentLevelPath;
    bool bFromEditor = false;
    FString EditorLevelJsonPath;

    AGridManager* FindOrSpawnGridManager();
    void ExecuteLoadLevel(const FString& JsonPath);
    void SetUIInputMode(bool bUIMode);
};
