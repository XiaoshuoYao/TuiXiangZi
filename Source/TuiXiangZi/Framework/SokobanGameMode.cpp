#include "Framework/SokobanGameMode.h"
#include "Framework/SokobanGameState.h"
#include "Framework/SokobanGameInstance.h"
#include "Grid/GridManager.h"
#include "Gameplay/SokobanCharacter.h"
#include "Gameplay/PushableBoxComponent.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "LevelData/LevelSerializer.h"
#include "LevelData/LevelDataTypes.h"
#include "UI/PauseMenuWidget.h"
#include "Tutorial/TutorialSubsystem.h"
#include "Tutorial/TutorialDataAsset.h"
#include "UI/TutorialWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"

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

    // InitFromLevelData handles cells, mechanisms, AND boxes
    GridManagerRef->InitFromLevelData(LevelData);

    // Position player
    UWorld* World = GetWorld();
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

    // Start tutorial for this level (skip when testing from editor or level already completed)
    if (!bFromEditor)
    {
        bool bAlreadyCompleted = false;
        if (USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance()))
        {
            FString FileName = FPaths::GetCleanFilename(JsonPath);
            bAlreadyCompleted = GI->IsPresetLevelCompleted(FileName);
        }

        if (!bAlreadyCompleted)
        {
            if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
            {
                TutSub->SetTutorialConfig(TutorialData, TutorialWidgetClass);
                TutSub->StartTutorial(CurrentLevelIndex);
            }
        }
    }
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
                CurrentLevelIndex = NextIndex;
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
        if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
        {
            TutSub->NotifyCondition(ETutorialConditionType::OnReset);
            TutSub->DismissTutorial();
        }
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

    if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
    {
        TutSub->NotifyCondition(ETutorialConditionType::OnUndo);
    }
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

        UE_LOG(LogTemp, Warning, TEXT("SokobanGameMode: bFromEditor = %s"), bFromEditor ? TEXT("true") : TEXT("false"));

        if (bFromEditor)
        {
            // Test mode: auto-return to editor after a short delay
            FTimerHandle TimerHandle;
            GetWorldTimerManager().SetTimer(TimerHandle, this,
                &ASokobanGameMode::ReturnToEditor, 1.0f, false);
        }
        else
        {
            ShowLevelCompleteMenu(Steps);
        }
    }
}

void ASokobanGameMode::UpdateBoxOnPlateVisuals()
{
    if (!GridManagerRef) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // Build map of plate positions to group colors
    TMap<FIntPoint, FLinearColor> PlateColorMap;
    for (const UGridMechanismComponent* Mech : GridManagerRef->GetAllMechanisms())
    {
        if (Mech && !Mech->BlocksPassage())
        {
            PlateColorMap.Add(Mech->GridPos,
                Mech->IsActivated() ? Mech->CachedActiveColor : Mech->CachedBaseColor);
        }
    }

    // Update each box via component
    for (UPushableBoxComponent* BoxComp : GridManagerRef->GetAllBoxes())
    {
        if (!BoxComp || !BoxComp->GetOwner() || BoxComp->GetOwner()->IsHidden()) continue;

        FLinearColor* Color = PlateColorMap.Find(BoxComp->CurrentGridPos);
        BoxComp->SetOnPlateVisual(Color != nullptr, Color ? *Color : FLinearColor::Black);
    }
}

void ASokobanGameMode::ReturnToEditor()
{
    FString Options;
    if (!EditorLevelJsonPath.IsEmpty())
    {
        Options = FString::Printf(TEXT("RestoreJson=%s"), *EditorLevelJsonPath);
    }
    UGameplayStatics::OpenLevel(this, FName(TEXT("EditorMap")), true, Options);
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
    // Test mode: ESC directly returns to editor
    if (bFromEditor)
    {
        ReturnToEditor();
        return;
    }

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

        if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
        {
            TutSub->SetPaused(true);
        }
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

    if (UTutorialSubsystem* TutSub = GetWorld()->GetSubsystem<UTutorialSubsystem>())
    {
        TutSub->SetPaused(false);
    }
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
