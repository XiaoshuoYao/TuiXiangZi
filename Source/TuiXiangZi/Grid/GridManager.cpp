#include "Grid/GridManager.h"
#include "Grid/TileStyleCatalog.h"
#include "LevelData/LevelDataTypes.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBox.h"
#include "Gameplay/Mechanisms/PressurePlate.h"
#include "Gameplay/Mechanisms/Door.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

AGridManager::AGridManager()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGridManager::BeginPlay() { Super::BeginPlay(); }
void AGridManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearGrid();
    Super::EndPlay(EndPlayReason);
}

// ---- 坐标转换 ----
FVector AGridManager::GridToWorld(FIntPoint GridPos) const
{
    return GridOrigin + FVector(
        GridPos.X * CellSize + CellSize * 0.5f,
        GridPos.Y * CellSize + CellSize * 0.5f,
        0.0f);
}

FIntPoint AGridManager::WorldToGrid(FVector WorldPos) const
{
    const FVector Relative = WorldPos - GridOrigin;
    return FIntPoint(
        FMath::FloorToInt(Relative.X / CellSize),
        FMath::FloorToInt(Relative.Y / CellSize));
}

// ---- 格子查询 ----
FGridCell AGridManager::GetCell(FIntPoint GridPos) const
{
    if (const FGridCell* Found = GridCells.Find(GridPos)) return *Found;
    return FGridCell(); // 默认 Empty
}

bool AGridManager::HasCell(FIntPoint GridPos) const { return GridCells.Contains(GridPos); }

bool AGridManager::IsCellPassable(FIntPoint GridPos) const
{
    if (!HasCell(GridPos)) return false;
    const FGridCell& Cell = GridCells[GridPos];
    switch (Cell.CellType)
    {
    case EGridCellType::Wall: return false;
    case EGridCellType::Door: return Cell.bDoorOpen;
    case EGridCellType::Floor:
    case EGridCellType::PressurePlate:
    case EGridCellType::Ice:
    case EGridCellType::Goal: return true;
    default: return false;
    }
}

bool AGridManager::CanPushBoxTo(FIntPoint GridPos) const
{
    if (IsCellPassable(GridPos)) return true;
    if (!HasCell(GridPos)) return true; // Empty = 坑洞可填平
    return false;
}

bool AGridManager::IsCellOccupied(FIntPoint GridPos) const
{
    if (const FGridCell* Found = GridCells.Find(GridPos))
        return Found->OccupyingActor.IsValid();
    return false;
}

// ---- 格子增删 ----
void AGridManager::SetCell(FIntPoint GridPos, const FGridCell& Cell)
{
    GridCells.Add(GridPos, Cell);
    SpawnOrUpdateVisualActor(GridPos, Cell);
}

void AGridManager::RemoveCell(FIntPoint GridPos)
{
    GridCells.Remove(GridPos);
    DestroyVisualActor(GridPos);
}

void AGridManager::SetCellOccupant(FIntPoint GridPos, AActor* Occupant)
{
    if (FGridCell* Cell = GridCells.Find(GridPos))
    {
        Cell->OccupyingActor = Occupant;
    }
}

// ---- 动态边界 ----
FIntRect AGridManager::GetGridBounds() const
{
    if (GridCells.Num() == 0) return FIntRect(FIntPoint(0,0), FIntPoint(0,0));
    FIntPoint Min(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
    FIntPoint Max(TNumericLimits<int32>::Min(), TNumericLimits<int32>::Min());
    for (const auto& Pair : GridCells)
    {
        Min.X = FMath::Min(Min.X, Pair.Key.X);
        Min.Y = FMath::Min(Min.Y, Pair.Key.Y);
        Max.X = FMath::Max(Max.X, Pair.Key.X);
        Max.Y = FMath::Max(Max.Y, Pair.Key.Y);
    }
    return FIntRect(Min, Max + FIntPoint(1, 1)); // Max exclusive
}

// ---- 关卡管理 ----
void AGridManager::InitFromLevelData(const FLevelData& Data)
{
    ClearGrid();
    GridOrigin = FVector::ZeroVector;
    for (const FCellData& CellData : Data.Cells)
    {
        FGridCell Cell;
        Cell.CellType = GridTypeUtils::StringToCellType(CellData.CellType);
        Cell.VisualStyleId = CellData.VisualStyleId;
        Cell.GroupId = CellData.GroupId;
        Cell.ExtraParam = CellData.ExtraParam;
        SetCell(CellData.GridPos, Cell);
        if (Cell.CellType == EGridCellType::Goal)
            GoalPositions.Add(CellData.GridPos);
    }
    SpawnMechanismActors(Data);
}

void AGridManager::InitEmptyGrid(int32 Width, int32 Height)
{
    ClearGrid();
    for (int32 Y = 0; Y < Height; ++Y)
        for (int32 X = 0; X < Width; ++X)
        {
            FGridCell Cell;
            Cell.CellType = EGridCellType::Floor;
            SetCell(FIntPoint(X, Y), Cell);
        }
}

void AGridManager::ClearGrid()
{
    DestroyAllVisualActors();
    // Destroy mechanism actors
    for (APressurePlate* P : AllPressurePlates) { if (P) P->Destroy(); }
    AllPressurePlates.Empty();
    for (ADoor* D : AllDoors) { if (D) D->Destroy(); }
    AllDoors.Empty();
    GridCells.Empty();
    GoalPositions.Empty();
}

// ---- 移动逻辑 ----
bool AGridManager::TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction)
{
    FGridCell* FromCell = GridCells.Find(FromGrid);
    if (!FromCell || !FromCell->OccupyingActor.IsValid()) return false;

    AActor* MovingActor = FromCell->OccupyingActor.Get();
    const FIntPoint Offset = DirectionToOffset(Direction);
    const FIntPoint TargetPos = FromGrid + Offset;

    // Target cell occupied? (box pushing)
    if (IsCellOccupied(TargetPos))
    {
        // Only player can push boxes
        ASokobanCharacter* PlayerChar = Cast<ASokobanCharacter>(MovingActor);
        if (!PlayerChar) return false;

        // Can't push box on closed door
        FGridCell* TargetCell = GridCells.Find(TargetPos);
        if (TargetCell && TargetCell->CellType == EGridCellType::Door && !TargetCell->bDoorOpen)
            return false;

        AActor* BoxActor = TargetCell->OccupyingActor.Get();
        FIntPoint BoxTargetPos = TargetPos + Offset;

        // Check if box can be pushed there
        if (!CanPushBoxTo(BoxTargetPos)) return false;

        // No chain pushing - if box target has a cell and it's occupied, fail
        if (HasCell(BoxTargetPos))
        {
            FGridCell* BoxTargetCell = GridCells.Find(BoxTargetPos);
            if (BoxTargetCell && BoxTargetCell->OccupyingActor.IsValid()) return false;
        }

        // === Execute box push ===

        if (!HasCell(BoxTargetPos))
        {
            // Box falls into pit
            TargetCell->OccupyingActor = nullptr; // clear box from current pos
            HandleBoxFallIntoPit(BoxActor, BoxTargetPos);
            OnActorLogicalMoved.Broadcast(BoxActor, TargetPos, BoxTargetPos);

            // Move player to TargetPos
            UpdateOccupancy(FromGrid, TargetPos, MovingActor);
            OnActorLogicalMoved.Broadcast(MovingActor, FromGrid, TargetPos);

            // Check if player's new position is ice
            if (TargetCell->CellType == EGridCellType::Ice)
            {
                FIntPoint SlideDest = CalculateIceSlideDestination(TargetPos, Direction);
                if (SlideDest != TargetPos)
                {
                    UpdateOccupancy(TargetPos, SlideDest, MovingActor);
                    OnActorLogicalMoved.Broadcast(MovingActor, TargetPos, SlideDest);
                }
            }
        }
        else
        {
            FGridCell* BoxTargetCell = GridCells.Find(BoxTargetPos);

            // Move box
            if (BoxTargetCell->CellType == EGridCellType::Ice)
            {
                // Box slides on ice
                UpdateOccupancy(TargetPos, BoxTargetPos, BoxActor);
                FIntPoint BoxFinalDest = CalculateIceSlideDestination(BoxTargetPos, Direction);
                if (BoxFinalDest != BoxTargetPos)
                {
                    UpdateOccupancy(BoxTargetPos, BoxFinalDest, BoxActor);
                    OnActorLogicalMoved.Broadcast(BoxActor, TargetPos, BoxFinalDest);
                }
                else
                {
                    OnActorLogicalMoved.Broadcast(BoxActor, TargetPos, BoxTargetPos);
                }
            }
            else
            {
                // Normal box move
                UpdateOccupancy(TargetPos, BoxTargetPos, BoxActor);
                OnActorLogicalMoved.Broadcast(BoxActor, TargetPos, BoxTargetPos);
            }

            // Move player to TargetPos (where box was)
            UpdateOccupancy(FromGrid, TargetPos, MovingActor);
            OnActorLogicalMoved.Broadcast(MovingActor, FromGrid, TargetPos);

            // Check if player's new position is ice
            FGridCell* PlayerNewCell = GridCells.Find(TargetPos);
            if (PlayerNewCell && PlayerNewCell->CellType == EGridCellType::Ice)
            {
                FIntPoint SlideDest = CalculateIceSlideDestination(TargetPos, Direction);
                if (SlideDest != TargetPos)
                {
                    UpdateOccupancy(TargetPos, SlideDest, MovingActor);
                    OnActorLogicalMoved.Broadcast(MovingActor, TargetPos, SlideDest);
                }
            }
        }
    }
    else
    {
        // No occupier at target
        if (!IsCellPassable(TargetPos)) return false;

        FGridCell* TargetCell = GridCells.Find(TargetPos);
        if (!TargetCell) return false;

        // Move actor
        UpdateOccupancy(FromGrid, TargetPos, MovingActor);
        OnActorLogicalMoved.Broadcast(MovingActor, FromGrid, TargetPos);

        // Ice slide
        if (TargetCell->CellType == EGridCellType::Ice)
        {
            FIntPoint SlideDest = CalculateIceSlideDestination(TargetPos, Direction);
            if (SlideDest != TargetPos)
            {
                UpdateOccupancy(TargetPos, SlideDest, MovingActor);
                OnActorLogicalMoved.Broadcast(MovingActor, TargetPos, SlideDest);
            }
        }
    }

    PostMoveSettlement();
    return true;
}

// ---- 移动辅助 ----
void AGridManager::UpdateOccupancy(FIntPoint OldPos, FIntPoint NewPos, AActor* Actor)
{
    if (FGridCell* OldCell = GridCells.Find(OldPos))
        OldCell->OccupyingActor = nullptr;
    if (FGridCell* NewCell = GridCells.Find(NewPos))
        NewCell->OccupyingActor = Actor;
    // Sync actor's cached grid pos
    if (ASokobanCharacter* Char = Cast<ASokobanCharacter>(Actor))
        Char->CurrentGridPos = NewPos;
    else if (APushableBox* Box = Cast<APushableBox>(Actor))
        Box->CurrentGridPos = NewPos;
}

void AGridManager::HandleBoxFallIntoPit(AActor* BoxActor, FIntPoint PitPos)
{
    // Create a new Floor cell at the pit position
    FGridCell NewFloorCell;
    NewFloorCell.CellType = EGridCellType::Floor;
    GridCells.Add(PitPos, NewFloorCell);
    SpawnOrUpdateVisualActor(PitPos, NewFloorCell);

    // Broadcast pit filled event
    OnPitFilled.Broadcast(PitPos, BoxActor);

    // Play fall animation on the box (hides and schedules destroy)
    if (APushableBox* Box = Cast<APushableBox>(BoxActor))
    {
        Box->PlayFallIntoHoleAnim();
    }
    else
    {
        BoxActor->SetActorHiddenInGame(true);
        BoxActor->SetLifeSpan(1.0f);
    }
}

FIntPoint AGridManager::CalculateIceSlideDestination(FIntPoint StartPos, EMoveDirection Direction)
{
    const FIntPoint Offset = DirectionToOffset(Direction);
    FIntPoint CurrentPos = StartPos;

    for (int32 Step = 0; Step < 100; ++Step)
    {
        FIntPoint NextPos = CurrentPos + Offset;
        if (!HasCell(NextPos)) break;                          // void/pit - stop
        const FGridCell& NextCell = GridCells[NextPos];
        if (!IsCellPassable(NextPos)) break;                   // impassable (wall, closed door) - stop
        if (NextCell.OccupyingActor.IsValid()) break;          // occupied - stop
        if (NextCell.CellType != EGridCellType::Ice) break;    // not ice - stop on current ice tile
        CurrentPos = NextPos;
    }

    return CurrentPos; // stays on last ice cell
}

void AGridManager::PostMoveSettlement()
{
    CheckAllPressurePlateGroups();
    CheckGoalCondition();
}

void AGridManager::CheckGoalCondition()
{
    for (const FIntPoint& GoalPos : GoalPositions)
    {
        if (const FGridCell* Cell = GridCells.Find(GoalPos))
        {
            if (Cell->OccupyingActor.IsValid())
            {
                if (Cast<ASokobanCharacter>(Cell->OccupyingActor.Get()))
                {
                    OnPlayerEnteredGoal.Broadcast(GoalPos);
                }
            }
        }
    }
}

// ---- 机关系统 ----
void AGridManager::CheckAllPressurePlateGroups()
{
    // Collect all unique group IDs
    TSet<int32> GroupIds;
    for (const APressurePlate* Plate : AllPressurePlates)
    {
        if (Plate && Plate->GroupId >= 0)
            GroupIds.Add(Plate->GroupId);
    }

    for (int32 GId : GroupIds)
    {
        bool bAllActive = true;
        for (APressurePlate* Plate : AllPressurePlates)
        {
            if (!Plate || Plate->GroupId != GId) continue;
            // Check if plate's grid position has an occupying actor
            const FGridCell* Cell = GridCells.Find(Plate->GridPos);
            bool bOccupied = Cell && Cell->OccupyingActor.IsValid();
            if (bOccupied)
                Plate->OnActivate();
            else
                Plate->OnDeactivate();
            if (!bOccupied) bAllActive = false;
        }

        // Update all doors in this group
        for (ADoor* Door : AllDoors)
        {
            if (!Door || Door->GroupId != GId) continue;
            if (bAllActive)
                Door->OnActivate();    // opens door
            else
                Door->OnDeactivate();  // closes door

            // Update grid cell door state
            if (FGridCell* DoorCell = GridCells.Find(Door->GridPos))
                DoorCell->bDoorOpen = bAllActive;
        }
    }
}

void AGridManager::SpawnMechanismActors(const FLevelData& Data)
{
    UWorld* World = GetWorld();
    if (!World) return;

    // Build GroupId -> Style lookup
    TMap<int32, FMechanismGroupStyleData> StyleMap;
    for (const auto& Style : Data.GroupStyles)
        StyleMap.Add(Style.GroupId, Style);

    for (const auto& Pair : GridCells)
    {
        const FIntPoint& Pos = Pair.Key;
        const FGridCell& Cell = Pair.Value;

        FVector WorldPos = GridToWorld(Pos);
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        if (Cell.CellType == EGridCellType::PressurePlate)
        {
            APressurePlate* Plate = World->SpawnActor<APressurePlate>(
                APressurePlate::StaticClass(), FTransform(WorldPos), SpawnParams);
            if (Plate)
            {
                Plate->GridPos = Pos;
                Plate->GroupId = Cell.GroupId;
                FLinearColor Base = FLinearColor(0.2f, 0.2f, 0.8f);
                FLinearColor Active = FLinearColor(0.4f, 0.4f, 1.0f);
                if (const FMechanismGroupStyleData* S = StyleMap.Find(Cell.GroupId))
                {
                    Base = S->BaseColor;
                    Active = S->ActiveColor;
                }
                else
                {
                    FRandomStream Rand(Cell.GroupId * 12345);
                    Base = FLinearColor(Rand.FRand(), Rand.FRand(), Rand.FRand());
                    Active = Base * 1.5f;
                }
                Plate->SetGroupColor(Base, Active);
                AllPressurePlates.Add(Plate);
            }
        }
        else if (Cell.CellType == EGridCellType::Door)
        {
            ADoor* DoorActor = World->SpawnActor<ADoor>(
                ADoor::StaticClass(), FTransform(WorldPos), SpawnParams);
            if (DoorActor)
            {
                DoorActor->GridPos = Pos;
                DoorActor->GroupId = Cell.GroupId;
                FLinearColor Base = FLinearColor(0.8f, 0.2f, 0.2f);
                FLinearColor Active = FLinearColor(1.0f, 0.4f, 0.4f);
                if (const FMechanismGroupStyleData* S = StyleMap.Find(Cell.GroupId))
                {
                    Base = S->BaseColor;
                    Active = S->ActiveColor;
                }
                else
                {
                    FRandomStream Rand(Cell.GroupId * 12345);
                    FLinearColor C(Rand.FRand(), Rand.FRand(), Rand.FRand());
                    Base = C;
                    Active = C * 1.5f;
                }
                DoorActor->SetGroupColor(Base, Active);
                DoorActor->SetDoorStateImmediate(false);
                AllDoors.Add(DoorActor);
            }
        }
    }
}

bool AGridManager::ExecuteSingleMove(FIntPoint FromGrid, FIntPoint ToGrid, EMoveDirection Direction)
{
    // Reserved for future use
    return false;
}

const FTileVisualStyle* AGridManager::ResolveTileVisual(const FGridCell& Cell) const
{
    if (!TileStyleCatalog) return nullptr;
    if (!Cell.VisualStyleId.IsNone())
    {
        if (const FTileVisualStyle* Style = TileStyleCatalog->FindStyle(Cell.VisualStyleId))
        {
            return Style;
        }
    }
    // Fallback: 返回该类型的第一个样式
    TArray<const FTileVisualStyle*> TypeStyles = TileStyleCatalog->GetStylesForType(Cell.CellType);
    return TypeStyles.Num() > 0 ? TypeStyles[0] : nullptr;
}

// ---- 可视化 Actor 管理 ----
void AGridManager::SpawnOrUpdateVisualActor(FIntPoint GridPos, const FGridCell& Cell)
{
    DestroyVisualActor(GridPos);
    if (Cell.CellType == EGridCellType::Empty) return;
    UWorld* World = GetWorld();
    if (!World) return;
    FVector WorldPos = GridToWorld(GridPos);
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* VisualActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(WorldPos), SpawnParams);
    if (!VisualActor) return;
    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(VisualActor);
    MeshComp->RegisterComponent();
    VisualActor->SetRootComponent(MeshComp);

    // Goal / PressurePlate / Door 等特殊格子底下也需要地板，视觉上按 Floor 处理
    const bool bUseFloorVisual = (Cell.CellType == EGridCellType::Goal
        || Cell.CellType == EGridCellType::PressurePlate
        || Cell.CellType == EGridCellType::Door);

    // 用于样式查询的 Cell：特殊格子查 Floor 样式
    FGridCell VisualCell = Cell;
    if (bUseFloorVisual)
    {
        VisualCell.CellType = EGridCellType::Floor;
    }

    // 优先使用 TileStyleCatalog 中配置的 Mesh 和 Material
    const FTileVisualStyle* Style = ResolveTileVisual(VisualCell);
    if (Style && Style->Mesh)
    {
        MeshComp->SetStaticMesh(Style->Mesh);
        if (Style->Material)
        {
            MeshComp->SetMaterial(0, Style->Material);
        }
    }
    else
    {
        // Fallback: 引擎基础 mesh + 动态着色
        UStaticMesh* Mesh = GetDefaultMeshForCellType(VisualCell.CellType);
        if (Mesh) MeshComp->SetStaticMesh(Mesh);
        UMaterialInterface* BaseMaterial = GetDefaultMaterialForCellType(VisualCell.CellType);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, VisualActor);
            FLinearColor Color;
            switch (VisualCell.CellType)
            {
            case EGridCellType::Floor: Color = FLinearColor(0.6f, 0.6f, 0.55f); break;
            case EGridCellType::Wall:  Color = FLinearColor(0.3f, 0.25f, 0.2f); break;
            case EGridCellType::Ice:   Color = FLinearColor(0.5f, 0.8f, 0.95f); break;
            case EGridCellType::Goal:  Color = FLinearColor(0.9f, 0.85f, 0.2f); break;
            default:                   Color = FLinearColor(0.5f, 0.5f, 0.5f); break;
            }
            DynMat->SetVectorParameterValue(FName("Color"), Color);
            MeshComp->SetMaterial(0, DynMat);
        }
    }

    switch (VisualCell.CellType)
    {
    case EGridCellType::Wall:
        MeshComp->SetWorldScale3D(FVector(CellSize / 100.0f));
        VisualActor->SetActorLocation(WorldPos + FVector(0, 0, CellSize * 0.5f));
        break;
    default:
        MeshComp->SetWorldScale3D(FVector(CellSize / 100.0f, CellSize / 100.0f, 1.0f));
        VisualActor->SetActorLocation(WorldPos);
        break;
    }

    // Goal 格子在地板之上额外叠加一个目标标记
    if (Cell.CellType == EGridCellType::Goal)
    {
        UStaticMeshComponent* GoalComp = NewObject<UStaticMeshComponent>(VisualActor);
        GoalComp->RegisterComponent();
        GoalComp->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform);

        const FTileVisualStyle* GoalStyle = ResolveTileVisual(Cell); // 用原始 Goal 类型查样式
        if (GoalStyle && GoalStyle->Mesh)
        {
            GoalComp->SetStaticMesh(GoalStyle->Mesh);
            if (GoalStyle->Material)
            {
                GoalComp->SetMaterial(0, GoalStyle->Material);
            }
        }
        else
        {
            // Fallback: 黄色平面标记
            UStaticMesh* GoalMesh = GetDefaultMeshForCellType(EGridCellType::Goal);
            if (GoalMesh) GoalComp->SetStaticMesh(GoalMesh);
            UMaterialInterface* GoalBaseMat = GetDefaultMaterialForCellType(EGridCellType::Goal);
            if (GoalBaseMat)
            {
                UMaterialInstanceDynamic* GoalDynMat = UMaterialInstanceDynamic::Create(GoalBaseMat, VisualActor);
                GoalDynMat->SetVectorParameterValue(FName("Color"), FLinearColor(0.9f, 0.85f, 0.2f));
                GoalComp->SetMaterial(0, GoalDynMat);
            }
        }
        // 略微抬高避免 Z-fighting
        GoalComp->SetWorldScale3D(FVector(CellSize / 100.0f, CellSize / 100.0f, 1.0f));
        GoalComp->SetRelativeLocation(FVector(0, 0, 1.0f));
    }

    VisualActors.Add(GridPos, VisualActor);
}

void AGridManager::DestroyVisualActor(FIntPoint GridPos)
{
    if (AActor** Found = VisualActors.Find(GridPos))
    {
        if (*Found) (*Found)->Destroy();
        VisualActors.Remove(GridPos);
    }
}

void AGridManager::DestroyAllVisualActors()
{
    for (auto& Pair : VisualActors)
        if (Pair.Value) Pair.Value->Destroy();
    VisualActors.Empty();
}

UStaticMesh* AGridManager::GetDefaultMeshForCellType(EGridCellType CellType) const
{
    switch (CellType)
    {
    case EGridCellType::Wall:
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    default:
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    }
}

UMaterialInterface* AGridManager::GetDefaultMaterialForCellType(EGridCellType CellType) const
{
    // 使用引擎自带的基础材质，为不同地块类型设定不同颜色
    // 颜色通过 SpawnOrUpdateVisualActor 中的动态材质实例设置
    return LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}
