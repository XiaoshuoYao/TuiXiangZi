#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Editor/EditorBrushTypes.h"
#include "Grid/GridTypes.h"
#include "LevelData/LevelDataTypes.h"
#include "LevelEditorGameMode.generated.h"

class AGridManager;
class AEditorGridVisualizer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrushChanged, EEditorBrush, NewBrush);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEditorModeChanged, EEditorMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupCreated, int32, GroupId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupDeleted, int32, GroupId);

USTRUCT(BlueprintType)
struct FLevelValidationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Errors;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Warnings;

    bool HasErrors() const { return Errors.Num() > 0; }
    bool HasWarnings() const { return Warnings.Num() > 0; }
};

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

    UFUNCTION(BlueprintCallable, Category = "Editor")
    TArray<int32> GetAllGroupIds() const;

    UFUNCTION(BlueprintCallable, Category = "Editor")
    FMechanismGroupStyleData GetGroupStyle(int32 GroupId) const;

    UFUNCTION(BlueprintCallable, Category = "Editor")
    void SetGroupColor(int32 GroupId, FLinearColor BaseColor, FLinearColor ActiveColor);

    // ===== Mode =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void SetEditorMode(EEditorMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    EEditorMode GetEditorMode() const { return CurrentMode; }

    /** Cancel current placement mode, return to Normal. */
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void CancelPlacementMode();

    // ===== Safety Checks =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    bool ShouldConfirmErase(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Editor")
    FString GetEraseWarning(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Editor")
    FLevelValidationResult ValidateLevel() const;

    // ===== Status Info =====
    UFUNCTION(BlueprintCallable, Category = "Editor")
    FString GetStatusText() const;

    UFUNCTION(BlueprintCallable, Category = "Editor")
    int32 GetCellCount() const;

    UFUNCTION(BlueprintCallable, Category = "Editor")
    int32 GetBoxCount() const { return BoxSpawnPositions.Num(); }

    UFUNCTION(BlueprintCallable, Category = "Editor")
    int32 GetGroupCount() const { return GroupStyles.Num(); }

    // ===== Delegates =====
    UPROPERTY(BlueprintAssignable, Category = "Editor")
    FOnBrushChanged OnBrushChanged;

    UPROPERTY(BlueprintAssignable, Category = "Editor")
    FOnEditorModeChanged OnEditorModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Editor")
    FOnGroupCreated OnGroupCreated;

    UPROPERTY(BlueprintAssignable, Category = "Editor")
    FOnGroupDeleted OnGroupDeleted;

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

    /** Group style data tracked by the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    TArray<FMechanismGroupStyleData> GroupStyles;

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

    /** Handle click during PlacingPlatesForDoor / EditingDoorGroup modes. */
    void HandlePlateModePaint(FIntPoint Pos);

    /** Generate a default color for a new group. */
    FLinearColor GetDefaultGroupColor(int32 GroupId) const;

    /** Count plates for a given group. */
    int32 CountPlatesForGroup(int32 GroupId) const;
};
