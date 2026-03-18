#include "Framework/SokobanGameMode.h"
#include "Framework/SokobanGameState.h"
#include "Framework/SokobanGameInstance.h"
#include "Grid/GridManager.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBox.h"
#include "Gameplay/Mechanisms/PressurePlate.h"
#include "Gameplay/Mechanisms/Door.h"
#include "LevelData/LevelSerializer.h"
#include "LevelData/LevelDataTypes.h"
#include "UI/PauseMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

ASokobanGameMode::ASokobanGameMode()
{
    // Prefer Blueprint character (with InputAction, MappingContext config)
    static ConstructorHelpers::FClassFinder<APawn> CharacterBP(
        TEXT("/Game/Blueprints/BP_SokobanCharacter"));
    if (CharacterBP.Succeeded())
    {
        DefaultPawnClass = CharacterBP.Class;
    }
    else
    {
        DefaultPawnClass = ASokobanCharacter::StaticClass();
    }
    // Prefer Blueprint box (with MoveCurve config)
    static ConstructorHelpers::FClassFinder<APushableBox> BoxBP(
        TEXT("/Game/Blueprints/BP_PushableBox"));
    if (BoxBP.Succeeded())
    {
        PushableBoxClass = BoxBP.Class;
    }
    else
    {
        PushableBoxClass = APushableBox::StaticClass();
    }
    GameStateClass = ASokobanGameState::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();
}

void ASokobanGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    FString FromEditorStr = UGameplayStatics::ParseOption(Options, TEXT("FromEditor"));
    bFromEditor = FromEditorStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) || FromEditorStr.Equals(TEXT("1"));

    EditorLevelJsonPath = UGameplayStatics::ParseOption(Options, TEXT("LevelJson"));
}

void ASokobanGameMode::BeginPlay()
{
    Super::BeginPlay();

    GridManagerRef = FindOrSpawnGridManager();

    if (GridManagerRef)
    {
        GridManagerRef->OnPlayerEnteredGoal.AddUObject(this, &ASokobanGameMode::OnPlayerEnteredGoal);
    }

    // Ensure input mode is set to Game (in case we came from UI-only menu)
    if (UWorld* W = GetWorld())
    {
        if (APlayerController* PC = W->GetFirstPlayerController())
        {
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = false;

            // Bind ESC key to toggle pause menu
            PC->InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASokobanGameMode::TogglePauseMenu);
        }
    }

    // Check GameInstance for a selected level (from main menu)
    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (GI && !GI->SelectedLevelPath.IsEmpty())
    {
        // Use the level selected from the menu
        CurrentLevelIndex = GI->SelectedPresetIndex;
        ExecuteLoadLevel(GI->SelectedLevelPath);
        return;
    }

    // Auto-discover levels if no manual paths configured
    if (LevelJsonPaths.Num() == 0)
    {
        FString LevelDir = ULevelSerializer::GetDefaultLevelDirectory();
        TArray<FString> FileNames;
        ULevelSerializer::GetAvailableLevelFiles(FileNames);
        FileNames.Sort();
        for (const FString& FileName : FileNames)
        {
            LevelJsonPaths.Add(LevelDir / FileName);
        }
        UE_LOG(LogTemp, Log, TEXT("SokobanGameMode: Auto-discovered %d levels"), LevelJsonPaths.Num());
    }

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
    if (!World) return nullptr;

    AGridManager* Found = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(World, AGridManager::StaticClass()));
    if (Found) return Found;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    return World->SpawnActor<AGridManager>(AGridManager::StaticClass(), FTransform::Identity, SpawnParams);
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

    FLevelData LevelData;
    if (!ULevelSerializer::LoadFromJson(JsonPath, LevelData))
    {
        UE_LOG(LogTemp, Error, TEXT("SokobanGameMode: Failed to load level from '%s'"), *JsonPath);
        return;
    }

    CurrentLevelPath = JsonPath;

    ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    if (GS)
    {
        GS->ResetState();
    }

    // Destroy existing boxes
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
            if (Box) Box->Destroy();
        }
    }

    GridManagerRef->InitFromLevelData(LevelData);

    // Spawn boxes
    if (World)
    {
        for (const FIntPoint& BoxPos : LevelData.BoxPositions)
        {
            FVector WorldPos = GridManagerRef->GridToWorld(BoxPos);
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            UClass* BoxClass = PushableBoxClass ? PushableBoxClass.Get() : APushableBox::StaticClass();
            APushableBox* NewBox = World->SpawnActor<APushableBox>(
                BoxClass, FTransform(WorldPos), SpawnParams);
            if (NewBox)
            {
                NewBox->SnapToGridPos(BoxPos);
                GridManagerRef->SetCellOccupant(BoxPos, NewBox);
            }
        }
    }

    // Position player
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

    GridManagerRef->CheckAllPressurePlateGroups();
    UpdateBoxOnPlateVisuals();
}

void ASokobanGameMode::LoadNextLevel()
{
    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());

    // If launched from menu with a specific level source
    if (GI && !GI->SelectedLevelPath.IsEmpty())
    {
        if (GI->SelectedLevelSource == ELevelSourceType::Preset)
        {
            // Try to load next preset level
            int32 NextIndex = GI->SelectedPresetIndex + 1;
            TArray<FString> PresetPaths = GI->GetPresetLevelPaths();
            if (PresetPaths.IsValidIndex(NextIndex) && GI->IsPresetLevelUnlocked(NextIndex))
            {
                GI->SelectedPresetIndex = NextIndex;
                GI->SelectedLevelPath = PresetPaths[NextIndex];
                ExecuteLoadLevel(GI->SelectedLevelPath);
                return;
            }
        }
        // For custom levels or no more presets, return to menu
        ReturnToMainMenu();
        return;
    }

    // Fallback: original auto-discovery behavior
    CurrentLevelIndex++;
    if (LevelJsonPaths.IsValidIndex(CurrentLevelIndex))
    {
        ExecuteLoadLevel(LevelJsonPaths[CurrentLevelIndex]);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("SokobanGameMode: All levels completed!"));
        ReturnToMainMenu();
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
    if (!GS || !GS->CanUndo() || !GridManagerRef) return;

    FLevelSnapshot Snapshot = GS->PopSnapshot();
    GS->RestoreSnapshot(Snapshot, GridManagerRef);
    UpdateBoxOnPlateVisuals();
    OnStepCountChanged.Broadcast(GS->StepCount);
}

void ASokobanGameMode::OnPlayerEnteredGoal(FIntPoint GoalPos)
{
    ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    if (GS && !GS->bLevelCompleted)
    {
        GS->bLevelCompleted = true;
        int32 Steps = GS->GetStepCount();
        UE_LOG(LogTemp, Log, TEXT("SokobanGameMode: Level completed! Steps: %d"), Steps);

        // Report completion to GameInstance for progression tracking
        USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
        if (GI && GI->SelectedLevelSource == ELevelSourceType::Preset)
        {
            FString FileName = FPaths::GetCleanFilename(GI->SelectedLevelPath);
            GI->MarkPresetLevelCompleted(FileName);
        }

        OnLevelCompleted.Broadcast(Steps);

        ShowLevelCompleteMenu(Steps);
    }
}

void ASokobanGameMode::UpdateBoxOnPlateVisuals()
{
    if (!GridManagerRef) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const TArray<APressurePlate*>& Plates = GridManagerRef->GetAllPressurePlates();

    // Build map of plate positions to group colors
    TMap<FIntPoint, FLinearColor> PlateColorMap;
    for (const APressurePlate* Plate : Plates)
    {
        if (Plate)
        {
            PlateColorMap.Add(Plate->GridPos, Plate->IsActivated() ?
                Plate->CachedActiveColor : Plate->CachedBaseColor);
        }
    }

    // Update each box
    for (TActorIterator<APushableBox> It(World); It; ++It)
    {
        APushableBox* Box = *It;
        if (!Box || Box->IsHidden()) continue;

        FLinearColor* Color = PlateColorMap.Find(Box->CurrentGridPos);
        Box->SetOnPlateVisual(Color != nullptr, Color ? *Color : FLinearColor::Black);
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

// ===== HUD Data Interface =====

int32 ASokobanGameMode::GetCurrentStepCount() const
{
    const ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    return GS ? GS->GetStepCount() : 0;
}

FString ASokobanGameMode::GetCurrentLevelName() const
{
    return FPaths::GetBaseFilename(CurrentLevelPath);
}

bool ASokobanGameMode::IsLevelCompleted() const
{
    const ASokobanGameState* GS = GetGameState<ASokobanGameState>();
    return GS ? GS->bLevelCompleted : false;
}

void ASokobanGameMode::RequestUndo() { UndoLastMove(); }
void ASokobanGameMode::RequestReset() { ResetCurrentLevel(); }
void ASokobanGameMode::RequestNextLevel() { LoadNextLevel(); }
void ASokobanGameMode::RequestReturnToMenu() { ReturnToMainMenu(); }

// ===== Pause Menu =====

void ASokobanGameMode::TogglePauseMenu()
{
    // If level is completed, ESC should not dismiss the menu
    if (bPauseMenuVisible && IsLevelCompleted())
    {
        return;
    }

    if (bPauseMenuVisible)
    {
        HidePauseMenu();
    }
    else
    {
        ShowPauseMenu();
    }
}

void ASokobanGameMode::ShowPauseMenu()
{
    if (bPauseMenuVisible) return;

    UWorld* W = GetWorld();
    if (!W) return;
    APlayerController* PC = W->GetFirstPlayerController();
    if (!PC) return;

    if (!PauseMenuWidget && PauseMenuWidgetClass)
    {
        PauseMenuWidget = CreateWidget<UPauseMenuWidget>(PC, PauseMenuWidgetClass);
    }

    if (PauseMenuWidget)
    {
        PauseMenuWidget->SetTitleText(FText::FromString(TEXT("暂停")));
        PauseMenuWidget->SetResumeButtonVisible(true);
        PauseMenuWidget->SetNextLevelButtonVisible(false);
        PauseMenuWidget->AddToViewport(100);
        bPauseMenuVisible = true;
        SetUIInputMode(true);
    }
}

void ASokobanGameMode::HidePauseMenu()
{
    if (!bPauseMenuVisible) return;

    if (PauseMenuWidget)
    {
        PauseMenuWidget->RemoveFromParent();
    }
    bPauseMenuVisible = false;
    SetUIInputMode(false);
}

void ASokobanGameMode::ShowLevelCompleteMenu(int32 Steps)
{
    UWorld* W = GetWorld();
    if (!W) return;
    APlayerController* PC = W->GetFirstPlayerController();
    if (!PC) return;

    if (!PauseMenuWidget && PauseMenuWidgetClass)
    {
        PauseMenuWidget = CreateWidget<UPauseMenuWidget>(PC, PauseMenuWidgetClass);
    }

    if (PauseMenuWidget)
    {
        FString Title = FString::Printf(TEXT("通关！步数：%d"), Steps);
        PauseMenuWidget->SetTitleText(FText::FromString(Title));
        PauseMenuWidget->SetResumeButtonVisible(false);
        PauseMenuWidget->SetNextLevelButtonVisible(true);
        PauseMenuWidget->AddToViewport(100);
        bPauseMenuVisible = true;
        SetUIInputMode(true);
    }
}

void ASokobanGameMode::SetUIInputMode(bool bUIMode)
{
    UWorld* W = GetWorld();
    if (!W) return;
    APlayerController* PC = W->GetFirstPlayerController();
    if (!PC) return;

    if (bUIMode)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = true;
    }
    else
    {
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
    }
}
