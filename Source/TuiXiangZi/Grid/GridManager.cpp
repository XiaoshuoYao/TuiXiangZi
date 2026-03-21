#include "Grid/GridManager.h"
#include "Grid/TileStyleCatalog.h"
#include "Grid/TileVisualActor.h"
#include "LevelData/LevelDataTypes.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBoxComponent.h"
#include "Gameplay/GridTileComponent.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "Gameplay/Mechanisms/DoorMechanismComponent.h"
#include "Gameplay/Mechanisms/GoalMechanismComponent.h"
#include "Gameplay/Modifiers/TileModifierComponent.h"
#include "Gameplay/GroupColorIndicatorComponent.h"
#include "DrawDebugHelpers.h"

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

// ---- Coordinate Conversion ----
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

// ---- Cell Query ----
FGridCell AGridManager::GetCell(FIntPoint GridPos) const
{
    if (const FGridCell* Found = GridCells.Find(GridPos)) return *Found;
    return FGridCell();
}

bool AGridManager::HasCell(FIntPoint GridPos) const { return GridCells.Contains(GridPos); }

bool AGridManager::IsCellPassable(FIntPoint GridPos) const
{
    if (!HasCell(GridPos)) return false;
    const FGridCell& Cell = GridCells[GridPos];
    // Door 的通行性由运行时状态决定
    if (Cell.CellType == EGridCellType::Door) return Cell.bDoorOpen;
    const FCellTypeDescriptor* Desc = GridTypeUtils::GetDescriptor(Cell.CellType);
    return Desc ? Desc->bPassable : false;
}

bool AGridManager::CanPushBoxTo(FIntPoint GridPos) const
{
    if (IsCellPassable(GridPos)) return true;
    if (!HasCell(GridPos)) return true;
    return false;
}

bool AGridManager::IsCellOccupied(FIntPoint GridPos) const
{
    if (const FGridCell* Found = GridCells.Find(GridPos))
        return Found->OccupyingActor.IsValid();
    return false;
}

// ---- Cell Edit ----
void AGridManager::SetCell(FIntPoint GridPos, const FGridCell& Cell)
{
    GridCells.Add(GridPos, Cell);
    SpawnOrUpdateVisualActor(GridPos, Cell);
    RefreshAdjacentDoors(GridPos);
}

void AGridManager::RemoveCell(FIntPoint GridPos)
{
    GridCells.Remove(GridPos);
    DestroyVisualActor(GridPos);
    RefreshAdjacentDoors(GridPos);
}

void AGridManager::SetCellOccupant(FIntPoint GridPos, AActor* Occupant)
{
    if (FGridCell* Cell = GridCells.Find(GridPos))
    {
        Cell->OccupyingActor = Occupant;
    }
}

void AGridManager::SetCellGroupId(FIntPoint GridPos, int32 GroupId)
{
    if (FGridCell* Cell = GridCells.Find(GridPos))
        Cell->GroupId = GroupId;

    if (const auto* Found = VisualActors.Find(GridPos))
    {
        if (*Found)
        {
            TArray<UGridTileComponent*> TileComps;
            (*Found)->GetComponents<UGridTileComponent>(TileComps);
            for (UGridTileComponent* Comp : TileComps)
                Comp->GroupId = GroupId;
        }
    }
}

ATileVisualActor* AGridManager::GetVisualActorAt(FIntPoint GridPos) const
{
    if (const auto* Found = VisualActors.Find(GridPos))
        return *Found;
    return nullptr;
}

// ---- Dynamic Bounds ----
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
    return FIntRect(Min, Max + FIntPoint(1, 1));
}

// ---- Level Management ----
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
    }
    ApplyMechanismGroupStyles(Data.GroupStyles);
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
    DestroyAllBoxActors();
    DestroyAllVisualActors();
    AllMechanisms.Empty();
    AllModifiers.Empty();
    ModifierLookup.Empty();
    GridCells.Empty();
}

// ---- Movement ----
bool AGridManager::TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction)
{
    FGridCell* FromCell = GridCells.Find(FromGrid);
    if (!FromCell || !FromCell->OccupyingActor.IsValid()) return false;

    AActor* MovingActor = FromCell->OccupyingActor.Get();
    const FIntPoint Offset = DirectionToOffset(Direction);
    const FIntPoint TargetPos = FromGrid + Offset;

    if (IsCellOccupied(TargetPos))
    {
        ASokobanCharacter* PlayerChar = Cast<ASokobanCharacter>(MovingActor);
        if (!PlayerChar) return false;

        FGridCell* TargetCell = GridCells.Find(TargetPos);
        if (TargetCell && TargetCell->CellType == EGridCellType::Door && !TargetCell->bDoorOpen)
            return false;

        AActor* BoxActor = TargetCell->OccupyingActor.Get();
        FIntPoint BoxTargetPos = TargetPos + Offset;

        if (!CanPushBoxTo(BoxTargetPos)) return false;

        if (HasCell(BoxTargetPos))
        {
            FGridCell* BoxTargetCell = GridCells.Find(BoxTargetPos);
            if (BoxTargetCell && BoxTargetCell->OccupyingActor.IsValid()) return false;
        }

        if (!HasCell(BoxTargetPos))
        {
            TargetCell->OccupyingActor = nullptr;
            HandleBoxFallIntoPit(BoxActor, BoxTargetPos);
            OnActorLogicalMoved.Broadcast(BoxActor, TargetPos, BoxTargetPos);

            UpdateOccupancy(FromGrid, TargetPos, MovingActor);
            OnActorLogicalMoved.Broadcast(MovingActor, FromGrid, TargetPos);

            if (GetModifierAt(TargetPos) && GetModifierAt(TargetPos)->ShouldContinueMovement(Direction))
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

            if (UTileModifierComponent* BoxMod = GetModifierAt(BoxTargetPos); BoxMod && BoxMod->ShouldContinueMovement(Direction))
            {
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
                UpdateOccupancy(TargetPos, BoxTargetPos, BoxActor);
                OnActorLogicalMoved.Broadcast(BoxActor, TargetPos, BoxTargetPos);
            }

            UpdateOccupancy(FromGrid, TargetPos, MovingActor);
            OnActorLogicalMoved.Broadcast(MovingActor, FromGrid, TargetPos);

            FGridCell* PlayerNewCell = GridCells.Find(TargetPos);
            if (UTileModifierComponent* PlayerMod = GetModifierAt(TargetPos); PlayerMod && PlayerMod->ShouldContinueMovement(Direction))
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
        if (!IsCellPassable(TargetPos)) return false;

        FGridCell* TargetCell = GridCells.Find(TargetPos);
        if (!TargetCell) return false;

        UpdateOccupancy(FromGrid, TargetPos, MovingActor);
        OnActorLogicalMoved.Broadcast(MovingActor, FromGrid, TargetPos);

        if (UTileModifierComponent* Mod = GetModifierAt(TargetPos); Mod && Mod->ShouldContinueMovement(Direction))
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

// ---- Movement Helpers ----
void AGridManager::UpdateOccupancy(FIntPoint OldPos, FIntPoint NewPos, AActor* Actor)
{
    if (FGridCell* OldCell = GridCells.Find(OldPos))
        OldCell->OccupyingActor = nullptr;
    if (FGridCell* NewCell = GridCells.Find(NewPos))
        NewCell->OccupyingActor = Actor;
    if (ASokobanCharacter* Char = Cast<ASokobanCharacter>(Actor))
        Char->CurrentGridPos = NewPos;
    else if (UPushableBoxComponent* BoxComp = Actor->FindComponentByClass<UPushableBoxComponent>())
        BoxComp->CurrentGridPos = NewPos;
}

void AGridManager::HandleBoxFallIntoPit(AActor* BoxActor, FIntPoint PitPos)
{
    FGridCell NewFloorCell;
    NewFloorCell.CellType = EGridCellType::Floor;
    GridCells.Add(PitPos, NewFloorCell);
    SpawnOrUpdateVisualActor(PitPos, NewFloorCell);

    OnPitFilled.Broadcast(PitPos, BoxActor);

    if (UPushableBoxComponent* BoxComp = BoxActor->FindComponentByClass<UPushableBoxComponent>())
    {
        BoxComp->PlayFallIntoHoleAnim();
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
        if (!HasCell(NextPos)) break;
        const FGridCell& NextCell = GridCells[NextPos];
        if (!IsCellPassable(NextPos)) break;
        if (NextCell.OccupyingActor.IsValid()) break;
        UTileModifierComponent* Mod = GetModifierAt(NextPos);
        if (!Mod || !Mod->ShouldContinueMovement(Direction)) break;
        CurrentPos = NextPos;
    }

    return CurrentPos;
}

void AGridManager::PostMoveSettlement()
{
    CheckAllPressurePlateGroups();
    CheckGoalCondition();
}

void AGridManager::CheckGoalCondition()
{
    for (UGridMechanismComponent* M : AllMechanisms)
    {
        UGoalMechanismComponent* GoalComp = Cast<UGoalMechanismComponent>(M);
        if (!GoalComp) continue;

        const FGridCell* Cell = GridCells.Find(GoalComp->GridPos);
        if (!Cell || !Cell->OccupyingActor.IsValid()) continue;

        if (Cast<ASokobanCharacter>(Cell->OccupyingActor.Get()))
        {
            OnPlayerEnteredGoal.Broadcast(GoalComp->GridPos);
        }
    }
}

// ---- Mechanism System ----
void AGridManager::CheckAllPressurePlateGroups()
{
    TSet<int32> GroupIds;
    for (UGridMechanismComponent* M : AllMechanisms)
    {
        if (M && M->GroupId >= 0 && M->IsGroupTrigger())
            GroupIds.Add(M->GroupId);
    }

    for (int32 GId : GroupIds)
    {
        bool bAllActive = true;

        // Check triggers (pressure plates)
        for (UGridMechanismComponent* M : AllMechanisms)
        {
            if (!M || M->GroupId != GId || !M->IsGroupTrigger()) continue;
            const FGridCell* Cell = GridCells.Find(M->GridPos);
            bool bOccupied = Cell && Cell->OccupyingActor.IsValid();
            bOccupied ? M->OnActivate() : M->OnDeactivate();
            if (!bOccupied) bAllActive = false;
        }

        // Activate/deactivate actuators (doors)
        for (UGridMechanismComponent* M : AllMechanisms)
        {
            if (!M || M->GroupId != GId || !M->BlocksPassage()) continue;
            bAllActive ? M->OnActivate() : M->OnDeactivate();
            if (FGridCell* MechCell = GridCells.Find(M->GridPos))
                MechCell->bDoorOpen = bAllActive;
        }
    }
}

UGridMechanismComponent* AGridManager::GetMechanismAt(FIntPoint GridPos) const
{
    for (UGridMechanismComponent* M : AllMechanisms)
    {
        if (M && M->GridPos == GridPos)
            return M;
    }
    return nullptr;
}

UTileModifierComponent* AGridManager::GetModifierAt(FIntPoint GridPos) const
{
    if (const auto* Found = ModifierLookup.Find(GridPos))
        return *Found;
    return nullptr;
}

void AGridManager::ApplyMechanismGroupStyles(const TArray<FMechanismGroupStyleData>& GroupStyles)
{
    TMap<int32, FMechanismGroupStyleData> StyleMap;
    for (const auto& S : GroupStyles)
        StyleMap.Add(S.GroupId, S);

    for (UGridMechanismComponent* Mech : AllMechanisms)
    {
        if (!Mech) continue;

        FLinearColor Base, Active;
        if (const auto* S = StyleMap.Find(Mech->GroupId))
        {
            Base = S->BaseColor;
            Active = S->ActiveColor;
        }
        else
        {
            FRandomStream Rand(Mech->GroupId * 12345);
            Base = FLinearColor(Rand.FRand(), Rand.FRand(), Rand.FRand());
            Active = Base * 1.5f;
        }
        Mech->SetGroupColor(Base, Active);

        if (AActor* MechOwner = Mech->GetOwner())
        {
            if (UGroupColorIndicatorComponent* Indicator = MechOwner->FindComponentByClass<UGroupColorIndicatorComponent>())
                Indicator->SetGroupColor(Base, Active);
        }

        if (UDoorMechanismComponent* DoorComp = Cast<UDoorMechanismComponent>(Mech))
            DoorComp->SetDoorStateImmediate(false);
    }
}

// ---- Box System ----
void AGridManager::SpawnBoxActorAt(FIntPoint GridPos, FName VisualStyleId)
{
    UWorld* World = GetWorld();
    if (!World) return;

    FVector WorldPos = GridToWorld(GridPos);
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    FGridCell BoxCell;
    BoxCell.CellType = EGridCellType::Box;
    BoxCell.VisualStyleId = VisualStyleId;

    ATileVisualActor* BoxActor = SpawnTileVisualFromStyle(BoxCell, WorldPos, SpawnParams);
    if (!BoxActor) return;

    // Fallback: if spawned actor has no mesh, set a default Cube
    if (BoxActor->MeshComp && !BoxActor->MeshComp->GetStaticMesh())
    {
        UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMesh)
            BoxActor->MeshComp->SetStaticMesh(CubeMesh);
    }

    BoxActor->InitializeForGrid(CellSize, GridPos);

    // Fallback: if no PushableBoxComponent in BP, add one dynamically
    UPushableBoxComponent* BoxComp = BoxActor->FindComponentByClass<UPushableBoxComponent>();
    if (!BoxComp)
    {
        BoxComp = NewObject<UPushableBoxComponent>(BoxActor, TEXT("PushableBoxComp"));
        BoxComp->RegisterComponent();
        BoxActor->AddOwnedComponent(BoxComp);
    }

    BoxComp->CurrentGridPos = GridPos;
    BoxComp->VisualStyleId = VisualStyleId;
    AllBoxes.Add(BoxComp);
    SetCellOccupant(GridPos, BoxActor);
}

void AGridManager::DestroyAllBoxActors()
{
    for (UPushableBoxComponent* BoxComp : AllBoxes)
    {
        if (BoxComp && BoxComp->GetOwner())
        {
            // Clear occupancy before destroying
            if (FGridCell* Cell = GridCells.Find(BoxComp->CurrentGridPos))
            {
                if (Cell->OccupyingActor == BoxComp->GetOwner())
                    Cell->OccupyingActor = nullptr;
            }
            BoxComp->GetOwner()->Destroy();
        }
    }
    AllBoxes.Empty();
}

UPushableBoxComponent* AGridManager::GetBoxComponentAt(FIntPoint GridPos) const
{
    for (UPushableBoxComponent* BoxComp : AllBoxes)
    {
        if (BoxComp && BoxComp->CurrentGridPos == GridPos)
            return BoxComp;
    }
    return nullptr;
}

bool AGridManager::ExecuteSingleMove(FIntPoint FromGrid, FIntPoint ToGrid, EMoveDirection Direction)
{
    return false;
}

const FTileVisualStyle* AGridManager::ResolveTileVisual(const FGridCell& Cell) const
{
    if (!TileStyleCatalog) return nullptr;
    if (!Cell.VisualStyleId.IsNone())
    {
        if (const FTileVisualStyle* Style = TileStyleCatalog->FindStyle(Cell.VisualStyleId))
        {
            // Only use this style if its type matches the cell type
            if (Style->ApplicableType == Cell.CellType)
                return Style;
            UE_LOG(LogTemp, Warning, TEXT("ResolveTileVisual: Style '%s' ApplicableType mismatch (style=%d, cell=%d), falling back"),
                *Cell.VisualStyleId.ToString(), (int)Style->ApplicableType, (int)Cell.CellType);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ResolveTileVisual: Style '%s' not found in catalog, falling back"),
                *Cell.VisualStyleId.ToString());
        }
    }
    // Fallback: first style for this cell type
    TArray<const FTileVisualStyle*> TypeStyles = TileStyleCatalog->GetStylesForType(Cell.CellType);
    return TypeStyles.Num() > 0 ? TypeStyles[0] : nullptr;
}

FName AGridManager::FindNearbyFloorStyleId(FIntPoint GridPos) const
{
    static const FIntPoint Offsets[] = {
        {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1}
    };
    for (const FIntPoint& Off : Offsets)
    {
        if (const FGridCell* Neighbor = GridCells.Find(GridPos + Off))
        {
            if (Neighbor->CellType == EGridCellType::Floor)
                return Neighbor->VisualStyleId;
        }
    }
    return NAME_None;
}

// ---- Visual Actor Management ----
ATileVisualActor* AGridManager::SpawnTileVisualFromStyle(
    const FGridCell& Cell, const FVector& WorldPos, FActorSpawnParameters& SpawnParams)
{
    TSubclassOf<AActor> ActorClass = nullptr;

    const FTileVisualStyle* Style = ResolveTileVisual(Cell);
    if (Style && Style->ActorClass)
        ActorClass = Style->ActorClass;

    if (!ActorClass)
    {
        if (auto* Found = DefaultVisualClasses.Find(Cell.CellType))
            ActorClass = *Found;
    }

    if (!ActorClass)
        ActorClass = ATileVisualActor::StaticClass();

    AActor* Spawned = GetWorld()->SpawnActor<AActor>(ActorClass, FTransform(WorldPos), SpawnParams);
    return Cast<ATileVisualActor>(Spawned);
}

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

    // Spawn floor underlay for special cell types (layered on top of floor)
    const FCellTypeDescriptor* CellDesc = GridTypeUtils::GetDescriptor(Cell.CellType);
    const bool bNeedsFloorUnderlay = CellDesc && CellDesc->bNeedsFloorUnderlay;

    if (bNeedsFloorUnderlay)
    {
        FGridCell FloorCell;
        FloorCell.CellType = EGridCellType::Floor;
        FloorCell.VisualStyleId = FindNearbyFloorStyleId(GridPos);

        ATileVisualActor* FloorVisual = SpawnTileVisualFromStyle(FloorCell, WorldPos, SpawnParams);
        if (FloorVisual)
        {
            FloorVisual->InitializeForGrid(CellSize, GridPos);
            FloorUnderlays.Add(GridPos, FloorVisual);
        }
    }

    // Box cells: spawn box actor separately (tracked via AllBoxes, not VisualActors)
    if (Cell.CellType == EGridCellType::Box)
    {
        SpawnBoxActorAt(GridPos, Cell.VisualStyleId);
        return;
    }

    ATileVisualActor* Visual = SpawnTileVisualFromStyle(Cell, WorldPos, SpawnParams);
    if (!Visual) return;

    // Auto-align door with adjacent walls
    if (Cell.CellType == EGridCellType::Door)
    {
        float Yaw = ComputeDoorYaw(GridPos);
        Visual->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
    }

    Visual->InitializeForGrid(CellSize, GridPos);
    VisualActors.Add(GridPos, Visual);

    // Register all grid tile components (mechanisms and modifiers)
    TArray<UGridTileComponent*> TileComps;
    Visual->GetComponents<UGridTileComponent>(TileComps);
    for (UGridTileComponent* Comp : TileComps)
    {
        Comp->GridPos = GridPos;
        Comp->GroupId = Cell.GroupId;

        if (UGridMechanismComponent* MechComp = Cast<UGridMechanismComponent>(Comp))
            AllMechanisms.Add(MechComp);
        if (UTileModifierComponent* ModComp = Cast<UTileModifierComponent>(Comp))
        {
            AllModifiers.Add(ModComp);
            ModifierLookup.Add(GridPos, ModComp);
        }
    }
}

float AGridManager::ComputeDoorYaw(FIntPoint GridPos) const
{
    auto IsWall = [this](FIntPoint Pos) -> bool
    {
        if (const FGridCell* Cell = GridCells.Find(Pos))
            return Cell->CellType == EGridCellType::Wall;
        return false;
    };

    // Coordinate convention: Left=(1,0), Right=(-1,0), Up=(0,1), Down=(0,-1)
    const bool bWallOnX = IsWall(GridPos + FIntPoint(1, 0)) || IsWall(GridPos + FIntPoint(-1, 0));
    const bool bWallOnY = IsWall(GridPos + FIntPoint(0, 1)) || IsWall(GridPos + FIntPoint(0, -1));

    if (bWallOnX && !bWallOnY)
        return 90.0f;  // Walls along X axis → door spans horizontally
    // Default: walls along Y axis or ambiguous
    return 0.0f;
}

void AGridManager::RefreshAdjacentDoors(FIntPoint GridPos)
{
    static const FIntPoint Offsets[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
    for (const FIntPoint& Off : Offsets)
    {
        FIntPoint Neighbor = GridPos + Off;
        if (const FGridCell* NCell = GridCells.Find(Neighbor))
        {
            if (NCell->CellType == EGridCellType::Door)
                SpawnOrUpdateVisualActor(Neighbor, *NCell);
        }
    }
}

void AGridManager::DestroyVisualActor(FIntPoint GridPos)
{
    // Destroy floor underlay if present
    if (ATileVisualActor** FloorFound = FloorUnderlays.Find(GridPos))
    {
        if (*FloorFound) (*FloorFound)->Destroy();
        FloorUnderlays.Remove(GridPos);
    }

    // Destroy box actor at this position if present
    if (UPushableBoxComponent* BoxComp = GetBoxComponentAt(GridPos))
    {
        if (BoxComp->GetOwner()) BoxComp->GetOwner()->Destroy();
        AllBoxes.Remove(BoxComp);
    }

    if (ATileVisualActor** Found = VisualActors.Find(GridPos))
    {
        if (*Found)
        {
            TArray<UGridMechanismComponent*> MechComps;
            (*Found)->GetComponents<UGridMechanismComponent>(MechComps);
            for (UGridMechanismComponent* Mech : MechComps)
                AllMechanisms.Remove(Mech);

            TArray<UTileModifierComponent*> ModComps;
            (*Found)->GetComponents<UTileModifierComponent>(ModComps);
            for (UTileModifierComponent* Mod : ModComps)
            {
                AllModifiers.Remove(Mod);
                ModifierLookup.Remove(Mod->GridPos);
            }

            (*Found)->Destroy();
        }
        VisualActors.Remove(GridPos);
    }
}

void AGridManager::DestroyAllVisualActors()
{
    for (auto& Pair : FloorUnderlays)
        if (Pair.Value) Pair.Value->Destroy();
    FloorUnderlays.Empty();

    for (auto& Pair : VisualActors)
        if (Pair.Value) Pair.Value->Destroy();
    VisualActors.Empty();
}
