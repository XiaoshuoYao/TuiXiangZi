#include "Editor/LevelEditorGameMode.h"
#include "Editor/LevelEditorPawn.h"
#include "Editor/EditorGridVisualizer.h"
#include "Grid/GridManager.h"
#include "Grid/TileStyleCatalog.h"
#include "Grid/TileVisualActor.h"
#include "Grid/GridTypes.h"
#include "Gameplay/GridTileComponent.h"
#include "LevelData/LevelDataTypes.h"
#include "LevelData/LevelSerializer.h"
#include "Tutorial/TutorialSubsystem.h"
#include "Tutorial/TutorialDataAsset.h"
#include "UI/TutorialWidget.h"
#include "Framework/SokobanGameInstance.h"
#include "Framework/SokobanSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ALevelEditorGameMode::ALevelEditorGameMode()
{
    DefaultPawnClass = ALevelEditorPawn::StaticClass();
    PlayerStartPos = FIntPoint(0, 0);
    PlayerStartMarker = nullptr;
}

void ALevelEditorGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    PendingRestoreJsonPath = UGameplayStatics::ParseOption(Options, TEXT("RestoreJson"));
}

void ALevelEditorGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Find or spawn GridManager
    GridManagerRef = Cast<AGridManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    if (!GridManagerRef)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        GridManagerRef = GetWorld()->SpawnActor<AGridManager>(
            AGridManager::StaticClass(), FTransform::Identity, SpawnParams);
    }

    // Spawn grid visualizer
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        GridVisualizerRef = GetWorld()->SpawnActor<AEditorGridVisualizer>(
            AEditorGridVisualizer::StaticClass(), FTransform::Identity, SpawnParams);
    }

    // Restore editor state if returning from test, otherwise create new level
    if (!PendingRestoreJsonPath.IsEmpty())
    {
        FLevelData Data;
        if (ULevelSerializer::LoadFromJson(PendingRestoreJsonPath, Data))
        {
            RestoreFromLevelData(Data);
            bIsDirty = true;
        }
        else
        {
            NewLevel(8, 6);
        }
    }
    else
    {
        NewLevel(8, 6);
    }

    // Ensure GridManager has a TileStyleCatalog (for packaged builds where the
    // dynamically spawned GridManager won't have it set via EditAnywhere).
    // LoadObject resolves from cooked content; assigned to GridManager's UPROPERTY to prevent GC.
    if (GridManagerRef && !GridManagerRef->TileStyleCatalog)
    {
        GridManagerRef->TileStyleCatalog = LoadObject<UTileStyleCatalog>(
            nullptr, TEXT("/Game/Misc/DA_DefaultTileStyles.DA_DefaultTileStyles"));
    }

    FocusEditorCamera();

    // Start editor tutorial (only on first entry)
    if (USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance()))
    {
        USokobanSaveGame* Save = GI->GetSaveGame();
        if (Save && !Save->bEditorTutorialCompleted)
        {
            if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
            {
                TutSub->SetTutorialConfig(TutorialData, TutorialWidgetClass);
                TutSub->StartEditorTutorial();
            }
            Save->bEditorTutorialCompleted = true;
            GI->SaveProgress();
        }
    }
}

void ALevelEditorGameMode::NotifyEditorTutorialEvent(FName EventTag)
{
    if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
    {
        TutSub->NotifyGameplayEvent(EventTag);
    }
}

void ALevelEditorGameMode::SetCurrentBrush(EEditorBrush NewBrush)
{
    if (CurrentBrush != NewBrush)
    {
        CurrentBrush = NewBrush;
        // Clear style selection when switching brush type to avoid cross-type mismatch
        CurrentVisualStyleId = NAME_None;
        OnBrushChanged.Broadcast(NewBrush);
        NotifyEditorTutorialEvent(FName(TEXT("BrushChanged")));
    }
}

void ALevelEditorGameMode::SetEditorMode(EEditorMode NewMode)
{
    if (CurrentMode != NewMode)
    {
        CurrentMode = NewMode;
        OnEditorModeChanged.Broadcast(NewMode);
        NotifyEditorTutorialEvent(FName(TEXT("ModeChanged")));
    }
}

void ALevelEditorGameMode::SetCurrentGroupId(int32 NewGroupId)
{
    CurrentGroupId = NewGroupId;
}

void ALevelEditorGameMode::CancelPlacementMode()
{
    if (CurrentMode == EEditorMode::PlacingPlatesForDoor)
    {
        // Warn if no plates were placed for the current group
        int32 PlateCount = CountPlatesForGroup(CurrentGroupId);
        if (PlateCount == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Editor: Door group %d has no plates!"), CurrentGroupId);
        }
    }
    SetEditorMode(EEditorMode::Normal);
}

EGridCellType ALevelEditorGameMode::BrushToCellType(EEditorBrush Brush) const
{
    EGridCellType Type = BrushUtils::BrushToCellType(Brush);
    // 共享查表默认返回 Empty，但编辑器中 fallback 为 Floor
    return (Type == EGridCellType::Empty && Brush != EEditorBrush::Eraser && Brush != EEditorBrush::PlayerStart)
        ? EGridCellType::Floor : Type;
}

void ALevelEditorGameMode::PaintAtGrid(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    // If in plate placement mode, handle differently
    if (CurrentMode == EEditorMode::PlacingPlatesForDoor || CurrentMode == EEditorMode::EditingDoorGroup)
    {
        HandlePlateModePaint(Pos);
        return;
    }

    switch (CurrentBrush)
    {
    case EEditorBrush::Floor:
    case EEditorBrush::Wall:
    case EEditorBrush::Ice:
    case EEditorBrush::Goal:
    case EEditorBrush::Door:
    {
        FGridCell Cell;
        Cell.CellType = BrushToCellType(CurrentBrush);
        Cell.VisualStyleId = CurrentVisualStyleId;
        GridManagerRef->SetCell(Pos, Cell);
        ApplyPostPaintFlow(Pos);
        break;
    }
    case EEditorBrush::PressurePlate:
    {
        FGridCell Cell;
        Cell.CellType = EGridCellType::PressurePlate;
        Cell.GroupId = CurrentGroupId;
        Cell.VisualStyleId = CurrentVisualStyleId;
        GridManagerRef->SetCell(Pos, Cell);
        GridManagerRef->ApplyMechanismGroupStyles(GroupStyles);
        break;
    }
    case EEditorBrush::BoxSpawn:
    {
        FGridCell Cell;
        Cell.CellType = EGridCellType::Box;
        Cell.VisualStyleId = CurrentVisualStyleId;
        GridManagerRef->SetCell(Pos, Cell);
        break;
    }
    case EEditorBrush::PlayerStart:
    {
        PlayerStartPos = Pos;
        SpawnPlayerStartMarker(Pos);
        if (!GridManagerRef->HasCell(Pos))
        {
            FGridCell Cell;
            Cell.CellType = EGridCellType::Floor;
            GridManagerRef->SetCell(Pos, Cell);
        }
        break;
    }
    case EEditorBrush::Eraser:
    {
        EraseAtGrid(Pos);
        break;
    }
    }

    bIsDirty = true;
    UpdateGridVisualizerBounds();
    NotifyEditorTutorialEvent(FName(TEXT("CellPainted")));
}

void ALevelEditorGameMode::HandlePlateModePaint(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    // Check if clicking an existing same-group plate -> toggle (remove it)
    if (GridManagerRef->HasCell(Pos))
    {
        FGridCell ExistingCell = GridManagerRef->GetCell(Pos);
        if (ExistingCell.CellType == EGridCellType::PressurePlate && ExistingCell.GroupId == CurrentGroupId)
        {
            // Remove the plate, replace with floor
            FGridCell FloorCell;
            FloorCell.CellType = EGridCellType::Floor;
            GridManagerRef->SetCell(Pos, FloorCell);
            UpdateGridVisualizerBounds();
            return;
        }

        // Clicking existing group anchor of same group -> ignore
        if (IsGroupAnchor(Pos) && ExistingCell.GroupId == CurrentGroupId)
        {
            return;
        }
    }

    // Only place on floor-type cells
    if (GridManagerRef->HasCell(Pos))
    {
        FGridCell ExistingCell = GridManagerRef->GetCell(Pos);
        if (ExistingCell.CellType != EGridCellType::Floor)
        {
            return; // Can only place plates on floor
        }
    }

    // Place pressure plate with current group
    FGridCell Cell;
    Cell.CellType = EGridCellType::PressurePlate;
    Cell.GroupId = CurrentGroupId;
    Cell.VisualStyleId = CurrentVisualStyleId;
    GridManagerRef->SetCell(Pos, Cell);
    GridManagerRef->ApplyMechanismGroupStyles(GroupStyles);
    UpdateGridVisualizerBounds();
}

void ALevelEditorGameMode::EraseAtGrid(FIntPoint Pos)
{
    if (!GridManagerRef) return;
    // Notify tutorial regardless of erase path
    NotifyEditorTutorialEvent(FName(TEXT("CellErased")));

    // Check if erasing a group anchor (any cell whose BP has a component with AssignGroup flow)
    if (IsGroupAnchor(Pos))
    {
        FGridCell Cell = GridManagerRef->GetCell(Pos);
        int32 GroupId = Cell.GroupId;
        DeleteGroup(GroupId);
        if (PlayerStartPos == Pos)
        {
            PlayerStartPos = FIntPoint(0, 0);
            if (PlayerStartMarker)
            {
                PlayerStartMarker->Destroy();
                PlayerStartMarker = nullptr;
            }
        }
        bIsDirty = true;
        UpdateGridVisualizerBounds();
        return;
    }

    // Check if the cell had a floor underlay (Wall, PressurePlate, etc.)
    // If so, replace with floor instead of removing entirely
    if (GridManagerRef->HasCell(Pos))
    {
        FGridCell Cell = GridManagerRef->GetCell(Pos);
        const FCellTypeDescriptor* Desc = GridTypeUtils::GetDescriptor(Cell.CellType);
        if (Desc && Desc->bEraseReplacesWithFloor)
        {
            FGridCell FloorCell;
            FloorCell.CellType = EGridCellType::Floor;
            FloorCell.VisualStyleId = GridManagerRef->FindNearbyFloorStyleId(Pos);
            GridManagerRef->SetCell(Pos, FloorCell);

            // Clear player start if it matches
            if (PlayerStartPos == Pos)
            {
                PlayerStartPos = FIntPoint(0, 0);
                if (PlayerStartMarker)
                {
                    PlayerStartMarker->Destroy();
                    PlayerStartMarker = nullptr;
                }
            }

            bIsDirty = true;
            UpdateGridVisualizerBounds();
            return;
        }
    }

    // Remove the grid cell (for Floor, Ice, etc.)
    GridManagerRef->RemoveCell(Pos);

    // Clear player start if it matches
    if (PlayerStartPos == Pos)
    {
        PlayerStartPos = FIntPoint(0, 0);
        if (PlayerStartMarker)
        {
            PlayerStartMarker->Destroy();
            PlayerStartMarker = nullptr;
        }
    }

    bIsDirty = true;
    UpdateGridVisualizerBounds();
}

bool ALevelEditorGameMode::ShouldConfirmErase(FIntPoint GridPos) const
{
    if (PlayerStartPos == GridPos) return true;
    if (GridManagerRef && GridManagerRef->HasCell(GridPos))
    {
        if (IsGroupAnchor(GridPos)) return true;
        FGridCell Cell = GridManagerRef->GetCell(GridPos);
        if (Cell.CellType == EGridCellType::Goal) return true;
    }
    return false;
}

FString ALevelEditorGameMode::GetEraseWarning(FIntPoint GridPos) const
{
    if (PlayerStartPos == GridPos)
        return TEXT("This cell contains the Player Start position!");

    if (GridManagerRef && GridManagerRef->HasCell(GridPos))
    {
        FGridCell Cell = GridManagerRef->GetCell(GridPos);
        if (IsGroupAnchor(GridPos))
            return FString::Printf(TEXT("Erasing this will delete the entire group %d (and all its plates)!"), Cell.GroupId);
        if (Cell.CellType == EGridCellType::Goal)
            return TEXT("This cell is the Goal!");
    }
    return FString();
}

FLevelValidationResult ALevelEditorGameMode::ValidateLevel() const
{
    FLevelValidationResult Result;

    // Error: No player start (check if it's on a valid cell)
    if (!GridManagerRef || !GridManagerRef->HasCell(PlayerStartPos))
    {
        Result.Errors.Add(TEXT("No valid Player Start position!"));
    }

    // Error: No goal
    bool bHasGoal = false;
    if (GridManagerRef)
    {
        FIntRect Bounds = GridManagerRef->GetGridBounds();
        for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
        {
            for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
            {
                FIntPoint Pos(X, Y);
                if (GridManagerRef->HasCell(Pos))
                {
                    FGridCell Cell = GridManagerRef->GetCell(Pos);
                    if (Cell.CellType == EGridCellType::Goal)
                    {
                        bHasGoal = true;
                        break;
                    }
                }
            }
            if (bHasGoal) break;
        }
    }
    if (!bHasGoal)
    {
        Result.Errors.Add(TEXT("No Goal cell found! The level needs at least one Goal."));
    }

    // Warning: Door without plates
    if (GridManagerRef)
    {
        // Collect all door group IDs
        TSet<int32> DoorGroups;
        TSet<int32> PlateGroups;
        FIntRect Bounds = GridManagerRef->GetGridBounds();
        for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
        {
            for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
            {
                FIntPoint Pos(X, Y);
                if (!GridManagerRef->HasCell(Pos)) continue;
                FGridCell Cell = GridManagerRef->GetCell(Pos);
                if (IsGroupAnchor(Pos)) DoorGroups.Add(Cell.GroupId);
                if (Cell.CellType == EGridCellType::PressurePlate) PlateGroups.Add(Cell.GroupId);
            }
        }
        for (int32 DoorGroup : DoorGroups)
        {
            if (!PlateGroups.Contains(DoorGroup))
            {
                Result.Warnings.Add(FString::Printf(TEXT("Door group %d has no pressure plates!"), DoorGroup));
            }
        }
    }

    // Warning: No boxes
    if (GetBoxCount() == 0)
    {
        Result.Warnings.Add(TEXT("No box spawn positions defined."));
    }

    return Result;
}

void ALevelEditorGameMode::NewLevel(int32 Width, int32 Height)
{
    if (!GridManagerRef) return;

    // Clear everything
    GridManagerRef->ClearGrid();

    // Destroy all markers
    if (PlayerStartMarker)
    {
        PlayerStartMarker->Destroy();
        PlayerStartMarker = nullptr;
    }
    // Reset editor state
    PlayerStartPos = FIntPoint(0, 0);
    CurrentGroupId = 0;
    MaxGroupId = 0;
    CurrentBrush = EEditorBrush::Floor;
    CurrentMode = EEditorMode::Normal;
    GroupStyles.Empty();

    // Create empty grid
    GridManagerRef->InitEmptyGrid(Width, Height);

    // Place default player start marker
    SpawnPlayerStartMarker(PlayerStartPos);

    bIsDirty = false;
    UpdateGridVisualizerBounds();
    FocusEditorCamera();
    NotifyEditorTutorialEvent(FName(TEXT("NewLevel")));
}

bool ALevelEditorGameMode::SaveLevel(const FString& FileName)
{
    // Validate before saving
    FLevelValidationResult Validation = ValidateLevel();
    if (Validation.HasErrors())
    {
        for (const FString& Error : Validation.Errors)
        {
            UE_LOG(LogTemp, Error, TEXT("Level Validation Error: %s"), *Error);
        }
        return false;
    }
    if (Validation.HasWarnings())
    {
        for (const FString& Warning : Validation.Warnings)
        {
            UE_LOG(LogTemp, Warning, TEXT("Level Validation Warning: %s"), *Warning);
        }
    }

    FLevelData Data = BuildLevelData();
    FString FilePath = ULevelSerializer::GetDefaultLevelDirectory() / FileName;

    if (!FilePath.EndsWith(TEXT(".json")))
    {
        FilePath += TEXT(".json");
    }

    if (ULevelSerializer::SaveToJson(Data, FilePath))
    {
        bIsDirty = false;
        NotifyEditorTutorialEvent(FName(TEXT("LevelSaved")));
        return true;
    }
    return false;
}

bool ALevelEditorGameMode::LoadLevel(const FString& FileName)
{
    FString FilePath = ULevelSerializer::GetDefaultLevelDirectory() / FileName;
    if (!FilePath.EndsWith(TEXT(".json")))
    {
        FilePath += TEXT(".json");
    }

    FLevelData Data;
    if (!ULevelSerializer::LoadFromJson(FilePath, Data))
    {
        return false;
    }

    RestoreFromLevelData(Data);
    bIsDirty = false;
    FocusEditorCamera();
    NotifyEditorTutorialEvent(FName(TEXT("LevelLoaded")));
    return true;
}

void ALevelEditorGameMode::RestoreFromLevelData(const FLevelData& Data)
{
    // Clear current state
    if (PlayerStartMarker)
    {
        PlayerStartMarker->Destroy();
        PlayerStartMarker = nullptr;
    }
    GroupStyles.Empty();

    // Load grid data
    if (GridManagerRef)
    {
        GridManagerRef->InitFromLevelData(Data);
    }

    // Restore editor state
    PlayerStartPos = Data.PlayerStart;
    GroupStyles = Data.GroupStyles;

    // Find max group ID from loaded data
    MaxGroupId = 0;
    for (const FCellData& CellData : Data.Cells)
    {
        if (CellData.GroupId > MaxGroupId)
        {
            MaxGroupId = CellData.GroupId;
        }
    }
    for (const FMechanismGroupStyleData& GS : GroupStyles)
    {
        if (GS.GroupId > MaxGroupId) MaxGroupId = GS.GroupId;
    }
    CurrentGroupId = 0;
    CurrentMode = EEditorMode::Normal;

    // Recreate player marker (box visuals are handled by grid cells)
    SpawnPlayerStartMarker(PlayerStartPos);

    UpdateGridVisualizerBounds();
}

void ALevelEditorGameMode::TestCurrentLevel()
{
    FLevelData Data = BuildLevelData();
    FString TempFilePath = ULevelSerializer::GetDefaultLevelDirectory() / TEXT("_temp_editor_test.json");
    ULevelSerializer::SaveToJson(Data, TempFilePath);

    NotifyEditorTutorialEvent(FName(TEXT("LevelTested")));
    UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("GameMap")),
        true, TEXT("FromEditor=true?LevelJson=") + TempFilePath);
}

int32 ALevelEditorGameMode::CreateNewGroup()
{
    MaxGroupId++;

    FMechanismGroupStyleData NewGroup;
    NewGroup.GroupId = MaxGroupId;
    NewGroup.BaseColor = GetDefaultGroupColor(MaxGroupId);
    NewGroup.ActiveColor = NewGroup.BaseColor * 1.3f; // Brighter active
    NewGroup.ActiveColor.A = 1.0f;
    NewGroup.DisplayName = FString::Printf(TEXT("Group %d"), MaxGroupId);
    GroupStyles.Add(NewGroup);

    CurrentGroupId = MaxGroupId;
    bIsDirty = true;
    OnGroupCreated.Broadcast(MaxGroupId);
    NotifyEditorTutorialEvent(FName(TEXT("GroupCreated")));
    return MaxGroupId;
}

void ALevelEditorGameMode::DeleteGroup(int32 GroupId)
{
    if (!GridManagerRef) return;

    // Remove all cells with matching GroupId
    FIntRect Bounds = GridManagerRef->GetGridBounds();
    TArray<FIntPoint> ToRemove;

    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
    {
        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
        {
            FIntPoint Pos(X, Y);
            if (GridManagerRef->HasCell(Pos))
            {
                FGridCell Cell = GridManagerRef->GetCell(Pos);
                if (Cell.GroupId == GroupId)
                {
                    ToRemove.Add(Pos);
                }
            }
        }
    }

    for (const FIntPoint& Pos : ToRemove)
    {
        // Replace with floor instead of removing entirely
        FGridCell FloorCell;
        FloorCell.CellType = EGridCellType::Floor;
        GridManagerRef->SetCell(Pos, FloorCell);
    }

    // Remove from GroupStyles
    GroupStyles.RemoveAll([GroupId](const FMechanismGroupStyleData& GS) { return GS.GroupId == GroupId; });

    bIsDirty = true;
    OnGroupDeleted.Broadcast(GroupId);
    UpdateGridVisualizerBounds();
}

TArray<int32> ALevelEditorGameMode::GetAllGroupIds() const
{
    TArray<int32> Ids;
    for (const FMechanismGroupStyleData& GS : GroupStyles)
    {
        Ids.Add(GS.GroupId);
    }
    return Ids;
}

FMechanismGroupStyleData ALevelEditorGameMode::GetGroupStyle(int32 GroupId) const
{
    for (const FMechanismGroupStyleData& GS : GroupStyles)
    {
        if (GS.GroupId == GroupId) return GS;
    }
    return FMechanismGroupStyleData();
}

void ALevelEditorGameMode::SetGroupColor(int32 GroupId, FLinearColor BaseColor, FLinearColor ActiveColor)
{
    for (FMechanismGroupStyleData& GS : GroupStyles)
    {
        if (GS.GroupId == GroupId)
        {
            GS.BaseColor = BaseColor;
            GS.ActiveColor = ActiveColor;
            bIsDirty = true;

            if (GridManagerRef)
                GridManagerRef->ApplyMechanismGroupStyles(GroupStyles);
            return;
        }
    }
}

void ALevelEditorGameMode::SetCurrentVisualStyleId(FName NewStyleId)
{
    CurrentVisualStyleId = NewStyleId;
}

UTileStyleCatalog* ALevelEditorGameMode::GetTileStyleCatalog() const
{
    return GridManagerRef ? GridManagerRef->TileStyleCatalog : nullptr;
}

int32 ALevelEditorGameMode::CountPlatesForGroup(int32 GroupId) const
{
    if (!GridManagerRef) return 0;

    int32 Count = 0;
    FIntRect Bounds = GridManagerRef->GetGridBounds();
    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
    {
        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
        {
            FIntPoint Pos(X, Y);
            if (GridManagerRef->HasCell(Pos))
            {
                FGridCell Cell = GridManagerRef->GetCell(Pos);
                if (Cell.CellType == EGridCellType::PressurePlate && Cell.GroupId == GroupId)
                {
                    Count++;
                }
            }
        }
    }
    return Count;
}

void ALevelEditorGameMode::ApplyPostPaintFlow(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    ATileVisualActor* Visual = GridManagerRef->GetVisualActorAt(Pos);
    if (!Visual) return;

    TArray<UGridTileComponent*> TileComps;
    Visual->GetComponents<UGridTileComponent>(TileComps);

    for (UGridTileComponent* M : TileComps)
    {
        EEditorPlacementFlow Flow = M->GetEditorPlacementFlow();
        if (Flow == EEditorPlacementFlow::AssignGroup)
        {
            int32 NewGroupId = CreateNewGroup();
            GridManagerRef->SetCellGroupId(Pos, NewGroupId);
            CurrentGroupId = NewGroupId;
            GridManagerRef->ApplyMechanismGroupStyles(GroupStyles);
            SetEditorMode(EEditorMode::PlacingPlatesForDoor);
            UE_LOG(LogTemp, Log, TEXT("Editor: Placed group anchor for group %d. Click to place plates, Esc to finish."), NewGroupId);
            break;
        }
    }
}

bool ALevelEditorGameMode::IsGroupAnchor(FIntPoint Pos) const
{
    if (!GridManagerRef) return false;

    ATileVisualActor* Visual = GridManagerRef->GetVisualActorAt(Pos);
    if (!Visual) return false;

    TArray<UGridTileComponent*> TileComps;
    Visual->GetComponents<UGridTileComponent>(TileComps);

    for (const UGridTileComponent* Comp : TileComps)
    {
        if (Comp->GetEditorPlacementFlow() == EEditorPlacementFlow::AssignGroup)
            return true;
    }
    return false;
}

FLinearColor ALevelEditorGameMode::GetDefaultGroupColor(int32 GroupId) const
{
    // Cycle through predefined colors
    static const FLinearColor Colors[] = {
        FLinearColor(0.8f, 0.2f, 0.2f), // Red
        FLinearColor(0.2f, 0.3f, 0.8f), // Blue
        FLinearColor(0.2f, 0.7f, 0.3f), // Green
        FLinearColor(0.6f, 0.2f, 0.7f), // Purple
        FLinearColor(0.9f, 0.6f, 0.1f), // Orange
        FLinearColor(0.1f, 0.7f, 0.7f), // Cyan
    };
    const int32 ColorCount = UE_ARRAY_COUNT(Colors);
    return Colors[(GroupId - 1) % ColorCount];
}

FString ALevelEditorGameMode::GetStatusText() const
{
    FString ModeStr;
    switch (CurrentMode)
    {
    case EEditorMode::Normal:
        ModeStr = TEXT("Normal");
        break;
    case EEditorMode::PlacingPlatesForDoor:
        ModeStr = FString::Printf(TEXT("Placing Plates for Group %d (Esc to finish)"), CurrentGroupId);
        break;
    case EEditorMode::EditingDoorGroup:
        ModeStr = FString::Printf(TEXT("Editing Group %d (Esc to finish)"), CurrentGroupId);
        break;
    }

    const FBrushDescriptor* BrushDesc = BrushUtils::GetDescriptor(CurrentBrush);
    FString BrushStr = BrushDesc ? BrushDesc->StatusName : TEXT("Unknown");

    return FString::Printf(TEXT("[%s] Brush: %s | Cells: %d | Boxes: %d | Groups: %d"),
        *ModeStr, *BrushStr, GetCellCount(), GetBoxCount(), GetGroupCount());
}

int32 ALevelEditorGameMode::GetCellCount() const
{
    if (!GridManagerRef) return 0;
    FIntRect Bounds = GridManagerRef->GetGridBounds();
    int32 Count = 0;
    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
    {
        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
        {
            if (GridManagerRef->HasCell(FIntPoint(X, Y))) Count++;
        }
    }
    return Count;
}

int32 ALevelEditorGameMode::GetBoxCount() const
{
    if (!GridManagerRef) return 0;
    int32 Count = 0;
    FIntRect Bounds = GridManagerRef->GetGridBounds();
    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
            if (GridManagerRef->HasCell(FIntPoint(X, Y))
                && GridManagerRef->GetCell(FIntPoint(X, Y)).CellType == EGridCellType::Box)
                Count++;
    return Count;
}

FLevelData ALevelEditorGameMode::BuildLevelData() const
{
    FLevelData Data;
    Data.PlayerStart = PlayerStartPos;
    Data.GroupStyles = GroupStyles;

    if (!GridManagerRef) return Data;

    FIntRect Bounds = GridManagerRef->GetGridBounds();
    for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
    {
        for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
        {
            FIntPoint Pos(X, Y);
            if (!GridManagerRef->HasCell(Pos)) continue;

            FGridCell Cell = GridManagerRef->GetCell(Pos);
            if (Cell.CellType == EGridCellType::Empty) continue;

            FCellData CellData;
            CellData.GridPos = Pos;
            CellData.CellType = GridTypeUtils::CellTypeToString(Cell.CellType);
            CellData.VisualStyleId = Cell.VisualStyleId;
            CellData.GroupId = Cell.GroupId;
            CellData.ExtraParam = Cell.ExtraParam;
            Data.Cells.Add(CellData);
        }
    }

    return Data;
}

void ALevelEditorGameMode::SpawnPlayerStartMarker(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    if (PlayerStartMarker)
    {
        PlayerStartMarker->Destroy();
        PlayerStartMarker = nullptr;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    FVector WorldPos = GridManagerRef->GridToWorld(Pos);
    WorldPos.Z = 5.0f;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Marker = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(WorldPos), SpawnParams);
    if (!Marker) return;

    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(Marker);
    MeshComp->RegisterComponent();
    Marker->SetRootComponent(MeshComp);

    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh)
    {
        MeshComp->SetStaticMesh(CylinderMesh);
    }

    float CellSize = GridManagerRef->CellSize;
    MeshComp->SetWorldScale3D(FVector(CellSize * 0.003f, CellSize * 0.003f, 0.01f));

    UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(
        MeshComp->GetMaterial(0), MeshComp);
    if (DynMat)
    {
        DynMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
        MeshComp->SetMaterial(0, DynMat);
    }

    Marker->SetActorLocation(WorldPos);
    PlayerStartMarker = Marker;
}

void ALevelEditorGameMode::UpdateGridVisualizerBounds()
{
    if (!GridVisualizerRef || !GridManagerRef) return;

    FIntRect Bounds = GridManagerRef->GetGridBounds();
    GridVisualizerRef->UpdateGridLines(Bounds.Min, Bounds.Max, GridManagerRef->CellSize);
}

void ALevelEditorGameMode::FocusEditorCamera()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("FocusEditorCamera: No PlayerController found!"));
        return;
    }

    ALevelEditorPawn* EditorPawn = Cast<ALevelEditorPawn>(PC->GetPawn());
    if (!EditorPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("FocusEditorCamera: No EditorPawn found! Pawn=%s"),
            PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("nullptr"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("FocusEditorCamera: Calling FocusCameraOnGrid on %s"), *EditorPawn->GetName());
    EditorPawn->FocusCameraOnGrid();
}
