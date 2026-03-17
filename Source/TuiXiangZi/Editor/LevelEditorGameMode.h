#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Editor/EditorBrushTypes.h"
#include "Grid/GridTypes.h"
#include "LevelEditorGameMode.generated.h"

class AGridManager;
class AEditorGridVisualizer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrushChanged, EEditorBrush, NewBrush);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditorModeChanged, EEditorMode, NewMode);

UCLASS(Blueprintable)
class TUIXIANGZI_API ALevelEditorGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALevelEditorGameMode();

    virtual void BeginPlay() override;

    // ===== Brush =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void SetCurrentBrush(EEditorBrush NewBrush);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    EEditorBrush GetCurrentBrush() const { return CurrentBrush; }

    // ===== Painting =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void PaintAtGrid(FIntPoint Pos);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    void EraseAtGrid(FIntPoint Pos);

    // ===== Level Operations =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void NewLevel(int32 Width, int32 Height);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    bool SaveLevel(const FString& FileName);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    bool LoadLevel(const FString& FileName);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    void TestCurrentLevel();

    // ===== Groups =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    int32 CreateNewGroup();

    UFUNCTION(BlueprintCallable, Category = "Editor")
    void DeleteGroup(int32 GroupId);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    void SetCurrentGroupId(int32 NewGroupId);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    int32 GetCurrentGroupId() const { return CurrentGroupId; }

    // ===== Mode =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void SetEditorMode(EEditorMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    EEditorMode GetEditorMode() const { return CurrentMode; }

    // ===== Delegates =====
    UPROPERTY(BlueprintAssignable, Category = "Editor")
    FOnBrushChanged OnBrushChanged;

    UPROPERTY(BlueprintAssignable, Category = "Editor")
    FOnEditorModeChanged OnEditorModeChanged;

    // ===== State =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    FIntPoint PlayerStartPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    TArray<FIntPoint> BoxSpawnPositions;

protected:
    UPROPERTY()
    AGridManager* GridManagerRef;

    UPROPERTY()
    AEditorGridVisualizer* GridVisualizerRef;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    EEditorBrush CurrentBrush = EEditorBrush::Floor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    EEditorMode CurrentMode = EEditorMode::Normal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    int32 CurrentGroupId = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    int32 MaxGroupId = 0;

    // ===== Marker Actors =====
    UPROPERTY()
    AActor* PlayerStartMarker;

    UPROPERTY()
    TMap<FIntPoint, AActor*> BoxSpawnMarkers;

    // ===== Helpers =====
    struct FLevelData BuildLevelData() const;
    void SpawnPlayerStartMarker(FIntPoint Pos);
    void SpawnBoxSpawnMarker(FIntPoint Pos);
    void RemoveBoxSpawnMarker(FIntPoint Pos);
    void UpdateGridVisualizerBounds();

    EGridCellType BrushToCellType(EEditorBrush Brush) const;
};
