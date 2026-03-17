#include "Editor/LevelEditorGameMode.h"
#include "Editor/LevelEditorPawn.h"
#include "Editor/EditorGridVisualizer.h"
#include "Grid/GridManager.h"
#include "Grid/GridTypes.h"
#include "LevelData/LevelDataTypes.h"
#include "LevelData/LevelSerializer.h"
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

    // Initialize with default level
    NewLevel(8, 6);
}

void ALevelEditorGameMode::SetCurrentBrush(EEditorBrush NewBrush)
{
    if (CurrentBrush != NewBrush)
    {
        CurrentBrush = NewBrush;
        OnBrushChanged.Broadcast(NewBrush);
    }
}

void ALevelEditorGameMode::SetEditorMode(EEditorMode NewMode)
{
    if (CurrentMode != NewMode)
    {
        CurrentMode = NewMode;
        OnEditorModeChanged.Broadcast(NewMode);
    }
}

void ALevelEditorGameMode::SetCurrentGroupId(int32 NewGroupId)
{
    CurrentGroupId = NewGroupId;
}

EGridCellType ALevelEditorGameMode::BrushToCellType(EEditorBrush Brush) const
{
    switch (Brush)
    {
    case EEditorBrush::Floor:          return EGridCellType::Floor;
    case EEditorBrush::Wall:           return EGridCellType::Wall;
    case EEditorBrush::Ice:            return EGridCellType::Ice;
    case EEditorBrush::Goal:           return EGridCellType::Goal;
    case EEditorBrush::Door:           return EGridCellType::Door;
    case EEditorBrush::PressurePlate:  return EGridCellType::PressurePlate;
    default:                           return EGridCellType::Floor;
    }
}

void ALevelEditorGameMode::PaintAtGrid(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    switch (CurrentBrush)
    {
    case EEditorBrush::Floor:
    case EEditorBrush::Wall:
    case EEditorBrush::Ice:
    case EEditorBrush::Goal:
    {
        FGridCell Cell;
        Cell.CellType = BrushToCellType(CurrentBrush);
        GridManagerRef->SetCell(Pos, Cell);
        break;
    }
    case EEditorBrush::Door:
    {
        FGridCell Cell;
        Cell.CellType = EGridCellType::Door;
        Cell.GroupId = CurrentGroupId;
        GridManagerRef->SetCell(Pos, Cell);
        break;
    }
    case EEditorBrush::PressurePlate:
    {
        FGridCell Cell;
        Cell.CellType = EGridCellType::PressurePlate;
        Cell.GroupId = CurrentGroupId;
        GridManagerRef->SetCell(Pos, Cell);
        break;
    }
    case EEditorBrush::BoxSpawn:
    {
        // Don't add duplicates
        if (!BoxSpawnPositions.Contains(Pos))
        {
            BoxSpawnPositions.Add(Pos);
            SpawnBoxSpawnMarker(Pos);
        }
        // Also ensure there's a floor tile underneath
        if (!GridManagerRef->HasCell(Pos))
        {
            FGridCell Cell;
            Cell.CellType = EGridCellType::Floor;
            GridManagerRef->SetCell(Pos, Cell);
        }
        break;
    }
    case EEditorBrush::PlayerStart:
    {
        PlayerStartPos = Pos;
        SpawnPlayerStartMarker(Pos);
        // Also ensure there's a floor tile underneath
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

    UpdateGridVisualizerBounds();
}

void ALevelEditorGameMode::EraseAtGrid(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    // Remove the grid cell
    GridManagerRef->RemoveCell(Pos);

    // Remove from box spawns if present
    BoxSpawnPositions.Remove(Pos);
    RemoveBoxSpawnMarker(Pos);

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

    UpdateGridVisualizerBounds();
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
    for (auto& Pair : BoxSpawnMarkers)
    {
        if (Pair.Value) Pair.Value->Destroy();
    }
    BoxSpawnMarkers.Empty();
    BoxSpawnPositions.Empty();

    // Reset editor state
    PlayerStartPos = FIntPoint(0, 0);
    CurrentGroupId = 0;
    MaxGroupId = 0;
    CurrentBrush = EEditorBrush::Floor;
    CurrentMode = EEditorMode::Normal;

    // Create empty grid
    GridManagerRef->InitEmptyGrid(Width, Height);

    // Place default player start marker
    SpawnPlayerStartMarker(PlayerStartPos);

    UpdateGridVisualizerBounds();
}

bool ALevelEditorGameMode::SaveLevel(const FString& FileName)
{
    FLevelData Data = BuildLevelData();
    FString FilePath = ULevelSerializer::GetDefaultLevelDirectory() / FileName;

    if (!FilePath.EndsWith(TEXT(".json")))
    {
        FilePath += TEXT(".json");
    }

    return ULevelSerializer::SaveToJson(Data, FilePath);
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

    // Clear current state
    if (PlayerStartMarker)
    {
        PlayerStartMarker->Destroy();
        PlayerStartMarker = nullptr;
    }
    for (auto& Pair : BoxSpawnMarkers)
    {
        if (Pair.Value) Pair.Value->Destroy();
    }
    BoxSpawnMarkers.Empty();
    BoxSpawnPositions.Empty();

    // Load grid data
    if (GridManagerRef)
    {
        GridManagerRef->InitFromLevelData(Data);
    }

    // Restore editor state
    PlayerStartPos = Data.PlayerStart;
    BoxSpawnPositions = Data.BoxPositions;

    // Find max group ID from loaded data
    MaxGroupId = 0;
    for (const FCellData& CellData : Data.Cells)
    {
        if (CellData.GroupId > MaxGroupId)
        {
            MaxGroupId = CellData.GroupId;
        }
    }
    CurrentGroupId = 0;

    // Recreate markers
    SpawnPlayerStartMarker(PlayerStartPos);
    for (const FIntPoint& BoxPos : BoxSpawnPositions)
    {
        SpawnBoxSpawnMarker(BoxPos);
    }

    UpdateGridVisualizerBounds();
    return true;
}

void ALevelEditorGameMode::TestCurrentLevel()
{
    // Save to temp file
    FLevelData Data = BuildLevelData();
    FString TempFilePath = ULevelSerializer::GetDefaultLevelDirectory() / TEXT("_temp_editor_test.json");
    ULevelSerializer::SaveToJson(Data, TempFilePath);

    // Open the game map with options pointing to the temp file
    UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("GameMap")),
        true, TEXT("LevelFile=_temp_editor_test.json"));
}

int32 ALevelEditorGameMode::CreateNewGroup()
{
    MaxGroupId++;
    CurrentGroupId = MaxGroupId;
    return MaxGroupId;
}

void ALevelEditorGameMode::DeleteGroup(int32 GroupId)
{
    if (!GridManagerRef) return;

    // Iterate all cells and remove those with matching GroupId
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
        GridManagerRef->RemoveCell(Pos);
    }

    UpdateGridVisualizerBounds();
}

FLevelData ALevelEditorGameMode::BuildLevelData() const
{
    FLevelData Data;
    Data.PlayerStart = PlayerStartPos;
    Data.BoxPositions = BoxSpawnPositions;

    if (!GridManagerRef) return Data;

    // Iterate grid bounds to collect all cells
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

    // Destroy old marker
    if (PlayerStartMarker)
    {
        PlayerStartMarker->Destroy();
        PlayerStartMarker = nullptr;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    FVector WorldPos = GridManagerRef->GridToWorld(Pos);
    WorldPos.Z = 5.0f; // Slightly above floor

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

    // Scale down and color green
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

void ALevelEditorGameMode::SpawnBoxSpawnMarker(FIntPoint Pos)
{
    if (!GridManagerRef) return;

    // Remove existing marker at this position
    RemoveBoxSpawnMarker(Pos);

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

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh)
    {
        MeshComp->SetStaticMesh(CubeMesh);
    }

    // Scale down and color orange
    float CellSize = GridManagerRef->CellSize;
    MeshComp->SetWorldScale3D(FVector(CellSize * 0.005f, CellSize * 0.005f, CellSize * 0.005f));

    UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(
        MeshComp->GetMaterial(0), MeshComp);
    if (DynMat)
    {
        DynMat->SetVectorParameterValue(FName("BaseColor"), FLinearColor(1.0f, 0.5f, 0.0f, 1.0f));
        MeshComp->SetMaterial(0, DynMat);
    }

    Marker->SetActorLocation(WorldPos);
    BoxSpawnMarkers.Add(Pos, Marker);
}

void ALevelEditorGameMode::RemoveBoxSpawnMarker(FIntPoint Pos)
{
    if (AActor** Found = BoxSpawnMarkers.Find(Pos))
    {
        if (*Found) (*Found)->Destroy();
        BoxSpawnMarkers.Remove(Pos);
    }
}

void ALevelEditorGameMode::UpdateGridVisualizerBounds()
{
    if (!GridVisualizerRef || !GridManagerRef) return;

    FIntRect Bounds = GridManagerRef->GetGridBounds();
    GridVisualizerRef->UpdateGridLines(Bounds.Min, Bounds.Max, GridManagerRef->CellSize);
}
