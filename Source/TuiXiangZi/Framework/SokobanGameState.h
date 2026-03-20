#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LevelData/LevelDataTypes.h"
#include "SokobanGameState.generated.h"

class AGridManager;

USTRUCT(BlueprintType)
struct FDoorSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FIntPoint DoorPos;
    UPROPERTY(BlueprintReadOnly) bool bDoorOpen = false;
};

USTRUCT(BlueprintType)
struct FPitSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FIntPoint PitPos;
    UPROPERTY(BlueprintReadOnly) bool bFilled = false;
};

USTRUCT(BlueprintType)
struct FLevelSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FIntPoint PlayerPos;
    UPROPERTY(BlueprintReadOnly) TArray<FBoxData> BoxStates;
    UPROPERTY(BlueprintReadOnly) TArray<FDoorSnapshot> DoorStates;
    UPROPERTY(BlueprintReadOnly) TArray<FPitSnapshot> PitStates;
    UPROPERTY(BlueprintReadOnly) int32 StepCount = 0;
};

UCLASS()
class TUIXIANGZI_API ASokobanGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    int32 StepCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
    bool bLevelCompleted = false;

    FLevelSnapshot CaptureSnapshot(const AGridManager* GM) const;
    void PushSnapshot(const FLevelSnapshot& Snapshot);
    FLevelSnapshot PopSnapshot();
    bool CanUndo() const;
    void RestoreSnapshot(const FLevelSnapshot& Snapshot, AGridManager* GM);
    void ResetState();
    void IncrementSteps();

    UFUNCTION(BlueprintCallable, Category = "Game")
    int32 GetStepCount() const;

private:
    TArray<FLevelSnapshot> SnapshotStack;
};
