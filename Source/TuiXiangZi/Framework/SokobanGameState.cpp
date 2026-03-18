#include "Framework/SokobanGameState.h"
#include "Framework/SokobanGameMode.h"
#include "Grid/GridManager.h"
#include "Grid/GridTypes.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBox.h"
#include "Gameplay/Mechanisms/Door.h"
#include "Gameplay/Mechanisms/PressurePlate.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

FLevelSnapshot ASokobanGameState::CaptureSnapshot(const AGridManager* GM) const
{
    FLevelSnapshot Snapshot;
    Snapshot.StepCount = StepCount;

    if (!GM)
    {
        return Snapshot;
    }

    // Find player position
    UWorld* World = GetWorld();
    if (World)
    {
        APlayerController* PC = World->GetFirstPlayerController();
        if (PC)
        {
            ASokobanCharacter* PlayerChar = Cast<ASokobanCharacter>(PC->GetPawn());
            if (PlayerChar)
            {
                Snapshot.PlayerPos = PlayerChar->CurrentGridPos;
            }
        }
    }

    // Collect box positions by iterating all APushableBox actors
    if (World)
    {
        for (TActorIterator<APushableBox> It(World); It; ++It)
        {
            APushableBox* Box = *It;
            if (Box && !Box->IsActorBeingDestroyed() && !Box->IsHidden())
            {
                Snapshot.BoxPositions.Add(Box->CurrentGridPos);
            }
        }
    }

    // Capture door states
    const TArray<ADoor*>& Doors = GM->GetAllDoors();
    for (const ADoor* Door : Doors)
    {
        if (Door)
        {
            FDoorSnapshot DS;
            DS.DoorPos = Door->GridPos;
            DS.bDoorOpen = Door->bIsOpen;
            Snapshot.DoorStates.Add(DS);
        }
    }

    // Capture pit states: cells that were originally Empty (pits) but have been filled to Floor
    // We track all cells that don't have a cell entry (true pits/voids) - but we can't iterate those.
    // Instead, we record positions that were pits and got filled. We use a convention:
    // Iterate GridCells and find Floor cells that might have been pits.
    // Since we can't distinguish original floors from filled pits easily, we store
    // the "filled pit" positions tracked by the GridManager's OnPitFilled events.
    // For undo, we rely on the GridManager's GridCells state:
    // We record all positions that currently exist as Floor but had no cell originally (Empty/void).
    // Actually, the simplest approach: record the CellType for every cell in the grid,
    // but that's too expensive. Instead, let's just not track pits for now and rely on
    // the full level reload for reset. For undo within a level, pits that get filled
    // are rare - we track them by checking which cells are Floor in the grid.
    //
    // Practical approach: We don't have original level data here, so we skip pit snapshots.
    // The undo system will work for all moves that don't involve pit-filling.
    // For pit-filling moves, the snapshot already captures box positions (the box that fell
    // is gone), so we need to track filled pits to properly undo.
    //
    // We'll record ALL cell positions and their types - but only for cells that could be pits.
    // Since filled pits become Floor cells at positions that originally had no cell,
    // we can't easily distinguish them. We'll record nothing for pits in this version
    // and handle it via the PitSnapshot mechanism below.

    // For now, pit snapshots are not populated - pit-filling moves cannot be undone.
    // A full reset (ResetCurrentLevel) will handle this case.

    return Snapshot;
}

void ASokobanGameState::PushSnapshot(const FLevelSnapshot& Snapshot)
{
    SnapshotStack.Add(Snapshot);
}

FLevelSnapshot ASokobanGameState::PopSnapshot()
{
    if (SnapshotStack.Num() > 0)
    {
        return SnapshotStack.Pop();
    }
    return FLevelSnapshot();
}

bool ASokobanGameState::CanUndo() const
{
    return SnapshotStack.Num() > 0;
}

void ASokobanGameState::RestoreSnapshot(const FLevelSnapshot& Snapshot, AGridManager* GM)
{
    if (!GM)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Step 1: Clear all OccupyingActor from cells (without re-spawning visuals)
    FIntRect Bounds = GM->GetGridBounds();
    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
    {
        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
        {
            FIntPoint Pos(X, Y);
            if (GM->HasCell(Pos))
            {
                GM->SetCellOccupant(Pos, nullptr);
            }
        }
    }

    // Step 2: Restore pit states (skip for now - pits are not tracked in snapshots)

    // Step 3: Destroy all current box actors
    TArray<APushableBox*> BoxesToDestroy;
    for (TActorIterator<APushableBox> It(World); It; ++It)
    {
        BoxesToDestroy.Add(*It);
    }
    for (APushableBox* Box : BoxesToDestroy)
    {
        if (Box)
        {
            Box->Destroy();
        }
    }

    // Step 4: Spawn new boxes at snapshot positions
    for (const FIntPoint& BoxPos : Snapshot.BoxPositions)
    {
        FVector WorldPos = GM->GridToWorld(BoxPos);
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        UClass* BoxClass = APushableBox::StaticClass();
        if (ASokobanGameMode* GM_Mode = Cast<ASokobanGameMode>(World->GetAuthGameMode()))
        {
            if (GM_Mode->PushableBoxClass)
                BoxClass = GM_Mode->PushableBoxClass.Get();
        }
        APushableBox* NewBox = World->SpawnActor<APushableBox>(
            BoxClass, FTransform(WorldPos), SpawnParams);
        if (NewBox)
        {
            NewBox->SnapToGridPos(BoxPos);
            GM->SetCellOccupant(BoxPos, NewBox);
        }
    }

    // Step 5: Restore player position
    APlayerController* PC = World->GetFirstPlayerController();
    if (PC)
    {
        ASokobanCharacter* PlayerChar = Cast<ASokobanCharacter>(PC->GetPawn());
        if (PlayerChar)
        {
            PlayerChar->SnapToGridPos(Snapshot.PlayerPos);
            GM->SetCellOccupant(Snapshot.PlayerPos, PlayerChar);
        }
    }

    // Step 6: Restore door states
    const TArray<ADoor*>& Doors = GM->GetAllDoors();
    for (const FDoorSnapshot& DS : Snapshot.DoorStates)
    {
        for (ADoor* Door : Doors)
        {
            if (Door && Door->GridPos == DS.DoorPos)
            {
                Door->SetDoorStateImmediate(DS.bDoorOpen);

                // Update grid cell door state via SetCell (doors need visual update)
                if (GM->HasCell(DS.DoorPos))
                {
                    FGridCell DoorCell = GM->GetCell(DS.DoorPos);
                    DoorCell.bDoorOpen = DS.bDoorOpen;
                    GM->SetCell(DS.DoorPos, DoorCell);
                }
                break;
            }
        }
    }

    // Step 7: Refresh all pressure plate visuals
    GM->CheckAllPressurePlateGroups();

    // Step 8: Restore StepCount
    StepCount = Snapshot.StepCount;
}

void ASokobanGameState::ResetState()
{
    StepCount = 0;
    bLevelCompleted = false;
    SnapshotStack.Empty();
}

void ASokobanGameState::IncrementSteps()
{
    StepCount++;
}

int32 ASokobanGameState::GetStepCount() const
{
    return StepCount;
}
