#include "Framework/SokobanGameMode.h"
#include "Framework/SokobanGameState.h"
#include "Grid/GridManager.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBox.h"
#include "LevelData/LevelSerializer.h"
#include "LevelData/LevelDataTypes.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ASokobanGameMode::ASokobanGameMode()
{
    DefaultPawnClass = ASokobanCharacter::StaticClass();
    GameStateClass = ASokobanGameState::StaticClass();
}

void ASokobanGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // Parse options for editor mode
    FString FromEditorStr = UGameplayStatics::ParseOption(Options, TEXT("FromEditor"));
    bFromEditor = FromEditorStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || FromEditorStr.Equals(TEXT("1"));

    EditorLevelJsonPath = UGameplayStatics::ParseOption(Options, TEXT("LevelJson"));
}

void ASokobanGameMode::BeginPlay()
{
    Super::BeginPlay();

    GridManagerRef = FindOrSpawnGridManager();

    // Bind to goal delegate
    if (GridManagerRef)
    {
        GridManagerRef->OnPlayerEnteredGoal.AddUObject(this, &ASokobanGameMode::OnPlayerEnteredGoal);
    }

    // Load level based on context
    if (bFromEditor && !EditorLevelJsonPath.IsEmpty())
    {
        ExecuteLoadLevel(EditorLevelJsonPath);
    }
    else if (LevelJsonPaths.Num() > 0 && LevelJsonPaths.IsValidIndex(CurrentLevelIndex))
    {
        ExecuteLoadLevel(LevelJsonPaths[CurrentLevelIndex]);
    }
}

AGridManager* ASokobanGameMode::FindOrSpawnGridManager()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Try to find an existing GridManager
    AGridManager* Found = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(World, AGridManager::StaticClass()));
    if (Found)
    {
        return Found;
    }

    // Spawn a new one
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AGridManager* NewGM = World->SpawnActor<AGridManager>(AGridManager::StaticClass(), FTransform::Identity, SpawnParams);
    return NewGM;
}

void ASokobanGameMode::LoadLevel(const FString& JsonFilePath)
{
    ExecuteLoadLevel(JsonFilePath);
}

void ASokobanGameMode::ExecuteLoadLevel(const FString& JsonPath)
{
    if (!GridManagerRef)
    {
        GridManagerRef = FindOrSpawnGridManager();
    }
    if (!GridManagerRef)
    {
        UE_LOG(LogTemp, Error, TEXT("SokobanGameMode: No GridManager available!"));
        return;
    }

    // Load level data from JSON
    FLevelData LevelData;
    if (!ULevelSerializer::LoadFromJson(JsonPath, LevelData))
    {
        UE_LOG(LogTemp, Error, TEXT("SokobanGameMode: Failed to load level from '%s'"), *JsonPath);
        return;
    }

    CurrentLevelPath = JsonPath;

    // Reset game state
    ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    if (GS)
    {
        GS->ResetState();
    }

    // Destroy existing boxes before InitFromLevelData clears the grid
    UWorld* World = GetWorld();
    if (World)
    {
        TArray<APushableBox*> OldBoxes;
        for (TActorIterator<APushableBox> It(World); It; ++It)
        {
            OldBoxes.Add(*It);
        }
        for (APushableBox* Box : OldBoxes)
        {
            if (Box)
            {
                Box->Destroy();
            }
        }
    }

    // Steps 1-6: GridManager handles cells, goals, mechanisms
    GridManagerRef->InitFromLevelData(LevelData);

    // Step 4 (from plan): Spawn boxes at BoxPositions + write OccupyingActor
    if (World)
    {
        for (const FIntPoint& BoxPos : LevelData.BoxPositions)
        {
            FVector WorldPos = GridManagerRef->GridToWorld(BoxPos);
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            APushableBox* NewBox = World->SpawnActor<APushableBox>(
                APushableBox::StaticClass(), FTransform(WorldPos), SpawnParams);
            if (NewBox)
            {
                NewBox->SnapToGridPos(BoxPos);
                GridManagerRef->SetCellOccupant(BoxPos, NewBox);
            }
        }
    }

    // Step 7: Spawn/move player at PlayerStart + write OccupyingActor
    if (World)
    {
        APlayerController* PC = World->GetFirstPlayerController();
        if (PC)
        {
            ASokobanCharacter* PlayerChar = Cast<ASokobanCharacter>(PC->GetPawn());
            if (PlayerChar)
            {
                PlayerChar->SnapToGridPos(LevelData.PlayerStart);
                GridManagerRef->SetCellOccupant(LevelData.PlayerStart, PlayerChar);
            }
        }
    }

    // Step 8: CheckAllPressurePlateGroups
    GridManagerRef->CheckAllPressurePlateGroups();

    // Step 9: CheckGoalCondition - handled by PostMoveSettlement internally
    // We don't call it here since the player just spawned and we don't want
    // an immediate goal trigger. The goal check happens after each move.
}

void ASokobanGameMode::LoadNextLevel()
{
    CurrentLevelIndex++;
    if (LevelJsonPaths.IsValidIndex(CurrentLevelIndex))
    {
        ExecuteLoadLevel(LevelJsonPaths[CurrentLevelIndex]);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("SokobanGameMode: No more levels! All levels completed."));
        // Could return to main menu or show completion screen
    }
}

void ASokobanGameMode::ResetCurrentLevel()
{
    if (!CurrentLevelPath.IsEmpty())
    {
        ExecuteLoadLevel(CurrentLevelPath);
    }
}

void ASokobanGameMode::UndoLastMove()
{
    ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    if (!GS || !GS->CanUndo())
    {
        return;
    }

    if (!GridManagerRef)
    {
        return;
    }

    FLevelSnapshot Snapshot = GS->PopSnapshot();
    GS->RestoreSnapshot(Snapshot, GridManagerRef);
}

void ASokobanGameMode::OnPlayerEnteredGoal(FIntPoint GoalPos)
{
    ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    if (GS && !GS->bLevelCompleted)
    {
        GS->bLevelCompleted = true;
        UE_LOG(LogTemp, Log, TEXT("SokobanGameMode: Level completed at goal (%d, %d)! Steps: %d"),
            GoalPos.X, GoalPos.Y, GS->GetStepCount());
    }
}

void ASokobanGameMode::ReturnToEditor()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("EditorMap")));
}

void ASokobanGameMode::ReturnToMainMenu()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("MainMenuMap")));
}
