#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridTypes.h"
#include "GridManager.generated.h"

struct FLevelData;
struct FTileVisualStyle;
struct FMechanismGroupStyleData;
class UTileStyleCatalog;
class ATileVisualActor;
class UGridTileComponent;
class UGridMechanismComponent;
class UDoorMechanismComponent;
class UTileModifierComponent;
struct FBoxData;
class ASokobanCharacter;
class UPushableBoxComponent;

UCLASS(BlueprintType)
class TUIXIANGZI_API AGridManager : public AActor
{
    GENERATED_BODY()

public:
    AGridManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ===== Grid Config =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
    float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
    FVector GridOrigin = FVector::ZeroVector;

    // ===== Coordinate Conversion =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
    FVector GridToWorld(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
    FIntPoint WorldToGrid(FVector WorldPos) const;

    // ===== Cell Query =====
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

    // ===== Cell Edit =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Edit")
    void SetCell(FIntPoint GridPos, const FGridCell& Cell);

    UFUNCTION(BlueprintCallable, Category = "Grid|Edit")
    void RemoveCell(FIntPoint GridPos);

    void SetCellOccupant(FIntPoint GridPos, AActor* Occupant);

    /** Update GroupId on both the cell data and any spawned mechanism components. */
    void SetCellGroupId(FIntPoint GridPos, int32 GroupId);

    /** Return the visual actor at a grid position, or nullptr. */
    ATileVisualActor* GetVisualActorAt(FIntPoint GridPos) const;

    // ===== Dynamic Bounds =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    FIntRect GetGridBounds() const;

    // ===== Level Management =====
    void InitFromLevelData(const FLevelData& Data);

    UFUNCTION(BlueprintCallable, Category = "Grid|Level")
    void InitEmptyGrid(int32 Width, int32 Height);

    UFUNCTION(BlueprintCallable, Category = "Grid|Level")
    void ClearGrid();

    // ===== Movement =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Gameplay")
    bool TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction);

    // ===== Mechanism System =====
    void CheckAllPressurePlateGroups();

    const TArray<UGridMechanismComponent*>& GetAllMechanisms() const { return AllMechanisms; }
    UGridMechanismComponent* GetMechanismAt(FIntPoint GridPos) const;

    void ApplyMechanismGroupStyles(const TArray<FMechanismGroupStyleData>& GroupStyles);

    // ===== Modifier System =====
    UTileModifierComponent* GetModifierAt(FIntPoint GridPos) const;

    // ===== Box System =====
    /** 在指定位置生成一个箱子 Actor（由 SpawnOrUpdateVisualActor 和 Undo 使用） */
    void SpawnBoxActorAt(FIntPoint GridPos, FName VisualStyleId);
    void DestroyAllBoxActors();
    const TArray<UPushableBoxComponent*>& GetAllBoxes() const { return AllBoxes; }
    UPushableBoxComponent* GetBoxComponentAt(FIntPoint GridPos) const;

    // ===== Style System =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Style")
    UTileStyleCatalog* TileStyleCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Style")
    TMap<EGridCellType, TSubclassOf<ATileVisualActor>> DefaultVisualClasses;

    const FTileVisualStyle* ResolveTileVisual(const FGridCell& Cell) const;

    /** Find the VisualStyleId of a nearby floor cell (for floor underlay matching). */
    FName FindNearbyFloorStyleId(FIntPoint GridPos) const;

protected:
    UPROPERTY(VisibleAnywhere, Category = "Grid|Debug")
    TMap<FIntPoint, FGridCell> GridCells;

    UPROPERTY()
    TMap<FIntPoint, ATileVisualActor*> VisualActors;

    /** 机关格子底下的地板 Actor */
    UPROPERTY()
    TMap<FIntPoint, ATileVisualActor*> FloorUnderlays;

    void SpawnOrUpdateVisualActor(FIntPoint GridPos, const FGridCell& Cell);
    void DestroyVisualActor(FIntPoint GridPos);
    void DestroyAllVisualActors();

    /** Compute yaw rotation for a door based on adjacent wall cells. */
    float ComputeDoorYaw(FIntPoint GridPos) const;

    /** Re-spawn any adjacent door visuals so they re-align with walls. */
    void RefreshAdjacentDoors(FIntPoint GridPos);

    ATileVisualActor* SpawnTileVisualFromStyle(const FGridCell& Cell, const FVector& WorldPos,
        FActorSpawnParameters& SpawnParams);

    // ===== Movement Helpers =====
    bool ExecuteSingleMove(FIntPoint FromGrid, FIntPoint ToGrid, EMoveDirection Direction);
    void HandleBoxFallIntoPit(AActor* BoxActor, FIntPoint PitPos);
    void UpdateOccupancy(FIntPoint OldPos, FIntPoint NewPos, AActor* Actor);
    FIntPoint CalculateIceSlideDestination(FIntPoint StartPos, EMoveDirection Direction);
    void PostMoveSettlement();
    void CheckGoalCondition();

    // ===== Mechanism Tracking =====
    UPROPERTY()
    TArray<UGridMechanismComponent*> AllMechanisms;

    // ===== Modifier Tracking =====
    UPROPERTY()
    TArray<UTileModifierComponent*> AllModifiers;

    TMap<FIntPoint, UTileModifierComponent*> ModifierLookup;

    // ===== Box Tracking =====
    UPROPERTY()
    TArray<UPushableBoxComponent*> AllBoxes;

    UPROPERTY(EditAnywhere, Category = "Grid|Debug")
    bool bDrawDebugGrid = true;

    UPROPERTY(EditAnywhere, Category = "Grid|Debug")
    FColor DebugGridColor = FColor::White;

    void DrawDebugGridLines() const;
};
