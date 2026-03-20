#include "Framework/SokobanGameState.h"
#include "Grid/GridManager.h"
#include "Grid/GridTypes.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBoxComponent.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "Gameplay/Mechanisms/DoorMechanismComponent.h"
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

    // Collect box states via component queries
    for (const UPushableBoxComponent* BoxComp : GM->GetAllBoxes())
    {
        if (BoxComp && BoxComp->GetOwner()
            && !BoxComp->GetOwner()->IsActorBeingDestroyed()
            && !BoxComp->GetOwner()->IsHidden())
        {
            FBoxData BD;
            BD.GridPos = BoxComp->CurrentGridPos;
            BD.VisualStyleId = BoxComp->VisualStyleId;
            Snapshot.BoxStates.Add(BD);
        }
    }

    // Capture door states
    for (const UGridMechanismComponent* Mech : GM->GetAllMechanisms())
    {
        if (const UDoorMechanismComponent* DoorComp = Cast<UDoorMechanismComponent>(Mech))
        {
            FDoorSnapshot DS;
            DS.DoorPos = DoorComp->GridPos;
            DS.bDoorOpen = DoorComp->bIsOpen;
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

    // Step 3+4: Destroy all boxes and respawn from snapshot
    GM->DestroyAllBoxActors();
    for (const FBoxData& BS : Snapshot.BoxStates)
    {
        GM->SpawnBoxActorAt(BS.GridPos, BS.VisualStyleId);
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
    for (const FDoorSnapshot& DS : Snapshot.DoorStates)
    {
        UGridMechanismComponent* Mech = GM->GetMechanismAt(DS.DoorPos);
        if (UDoorMechanismComponent* DoorComp = Cast<UDoorMechanismComponent>(Mech))
        {
            DoorComp->SetDoorStateImmediate(DS.bDoorOpen);

            if (GM->HasCell(DS.DoorPos))
            {
                FGridCell DoorCell = GM->GetCell(DS.DoorPos);
                DoorCell.bDoorOpen = DS.bDoorOpen;
                GM->SetCell(DS.DoorPos, DoorCell);
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
