#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SokobanGameMode.generated.h"

class AGridManager;

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

    void OnPlayerEnteredGoal(FIntPoint GoalPos);

protected:
    virtual void BeginPlay() override;
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
    TArray<FString> LevelJsonPaths;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    int32 CurrentLevelIndex = 0;

private:
    UPROPERTY()
    AGridManager* GridManagerRef = nullptr;

    FString CurrentLevelPath;
    bool bFromEditor = false;
    FString EditorLevelJsonPath;

    AGridManager* FindOrSpawnGridManager();
    void ExecuteLoadLevel(const FString& JsonPath);
};
