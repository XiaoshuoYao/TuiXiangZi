#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridTypes.h"
#include "GridManager.generated.h"

struct FLevelData;
struct FTileVisualStyle;
class UTileStyleCatalog;
class APressurePlate;
class ADoor;
class ASokobanCharacter;
class APushableBox;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActorLogicalMoved, AActor*, FIntPoint, FIntPoint);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerEnteredGoal, FIntPoint);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPitFilled, FIntPoint, AActor*);

UCLASS(BlueprintType)
class TUIXIANGZI_API AGridManager : public AActor
{
    GENERATED_BODY()

public:
    AGridManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ===== 网格配置 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
    float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
    FVector GridOrigin = FVector::ZeroVector;

    // ===== 坐标转换 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
    FVector GridToWorld(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
    FIntPoint WorldToGrid(FVector WorldPos) const;

    // ===== 格子查询 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    FGridCell GetCell(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool HasCell(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool IsCellPassable(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool CanPushBoxTo(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool IsCellOccupied(FIntPoint GridPos) const;

    // ===== 格子增删 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Edit")
    void SetCell(FIntPoint GridPos, const FGridCell& Cell);

    UFUNCTION(BlueprintCallable, Category = "Grid|Edit")
    void RemoveCell(FIntPoint GridPos);

    /** Set the OccupyingActor on a cell without re-spawning visual actors. */
    void SetCellOccupant(FIntPoint GridPos, AActor* Occupant);

    // ===== 动态边界 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    FIntRect GetGridBounds() const;

    // ===== 关卡管理 =====
    void InitFromLevelData(const FLevelData& Data);

    UFUNCTION(BlueprintCallable, Category = "Grid|Level")
    void InitEmptyGrid(int32 Width, int32 Height);

    UFUNCTION(BlueprintCallable, Category = "Grid|Level")
    void ClearGrid();

    // ===== 事件委托 =====
    FOnActorLogicalMoved OnActorLogicalMoved;
    FOnPlayerEnteredGoal OnPlayerEnteredGoal;
    FOnPitFilled OnPitFilled;

    // ===== 移动逻辑 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Gameplay")
    bool TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction);

    // ===== 机关系统 =====
    void CheckAllPressurePlateGroups();
    void SpawnMechanismActors(const FLevelData& Data);

    // ===== 机关 Actor 访问 =====
    const TArray<APressurePlate*>& GetAllPressurePlates() const { return AllPressurePlates; }
    const TArray<ADoor*>& GetAllDoors() const { return AllDoors; }

    // ===== 样式系统 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Style")
    UTileStyleCatalog* TileStyleCatalog;

    const FTileVisualStyle* ResolveTileVisual(const FGridCell& Cell) const;

protected:
    UPROPERTY(VisibleAnywhere, Category = "Grid|Debug")
    TMap<FIntPoint, FGridCell> GridCells;

    UPROPERTY(VisibleAnywhere, Category = "Grid|Debug")
    TArray<FIntPoint> GoalPositions;

    UPROPERTY()
    TMap<FIntPoint, AActor*> VisualActors;

    void SpawnOrUpdateVisualActor(FIntPoint GridPos, const FGridCell& Cell);
    void DestroyVisualActor(FIntPoint GridPos);
    void DestroyAllVisualActors();
    UStaticMesh* GetDefaultMeshForCellType(EGridCellType CellType) const;
    UMaterialInterface* GetDefaultMaterialForCellType(EGridCellType CellType) const;

    // ===== 移动辅助 =====
    bool ExecuteSingleMove(FIntPoint FromGrid, FIntPoint ToGrid, EMoveDirection Direction);
    void HandleBoxFallIntoPit(AActor* BoxActor, FIntPoint PitPos);
    void UpdateOccupancy(FIntPoint OldPos, FIntPoint NewPos, AActor* Actor);
    FIntPoint CalculateIceSlideDestination(FIntPoint StartPos, EMoveDirection Direction);
    void PostMoveSettlement();
    void CheckGoalCondition();

    // ===== 机关 Actor 追踪 =====
    UPROPERTY()
    TArray<APressurePlate*> AllPressurePlates;

    UPROPERTY()
    TArray<ADoor*> AllDoors;

    UPROPERTY(EditAnywhere, Category = "Grid|Debug")
    bool bDrawDebugGrid = true;

    UPROPERTY(EditAnywhere, Category = "Grid|Debug")
    FColor DebugGridColor = FColor::White;

    void DrawDebugGridLines() const;
};
