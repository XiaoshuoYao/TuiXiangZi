#include "Editor/LevelEditorPawn.h"
#include "Grid/GridManager.h"
#include "Editor/LevelEditorGameMode.h"
#include "Editor/EditorBrushTypes.h"
#include "UI/Editor/EditorMainWidget.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

ALevelEditorPawn::ALevelEditorPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Orthographic top-down camera as root
    EditorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("EditorCamera"));
    RootComponent = EditorCamera;

    EditorCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
    EditorCamera->OrthoWidth = 1024.0f;
    EditorCamera->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    SetActorLocation(FVector(0.0f, 0.0f, 1000.0f));

    AutoPossessAI = EAutoPossessAI::Disabled;
}

void ALevelEditorPawn::BeginPlay()
{
    Super::BeginPlay();

    SetActorLocation(FVector(400.0f, 300.0f, 1000.0f));

    // Cache GridManager reference
    GridManagerRef = Cast<AGridManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    // Show mouse cursor and enable click events
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;

        // Add Enhanced Input Mapping Context
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (EditorMappingContext)
            {
                Subsystem->AddMappingContext(EditorMappingContext, 0);
            }
        }
    }

    // Create Editor UI
    if (MainWidgetClass)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            MainWidget = CreateWidget<UEditorMainWidget>(PC, MainWidgetClass);
            if (MainWidget)
            {
                MainWidget->AddToViewport(0);
            }

            // Set input mode: game and UI coexist
            FInputModeGameAndUI InputMode;
            InputMode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(true);
        }
    }
}

void ALevelEditorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (LeftClickAction)
        {
            EnhancedInput->BindAction(LeftClickAction, ETriggerEvent::Started, this, &ALevelEditorPawn::OnLeftClickStarted);
            EnhancedInput->BindAction(LeftClickAction, ETriggerEvent::Completed, this, &ALevelEditorPawn::OnLeftClickCompleted);
        }
        if (RightClickAction)
        {
            EnhancedInput->BindAction(RightClickAction, ETriggerEvent::Started, this, &ALevelEditorPawn::OnRightClickStarted);
            EnhancedInput->BindAction(RightClickAction, ETriggerEvent::Completed, this, &ALevelEditorPawn::OnRightClickCompleted);
        }
        if (MiddleClickAction)
        {
            EnhancedInput->BindAction(MiddleClickAction, ETriggerEvent::Started, this, &ALevelEditorPawn::OnMiddleClickStarted);
            EnhancedInput->BindAction(MiddleClickAction, ETriggerEvent::Completed, this, &ALevelEditorPawn::OnMiddleClickCompleted);
        }
        if (MouseWheelAction)
        {
            EnhancedInput->BindAction(MouseWheelAction, ETriggerEvent::Triggered, this, &ALevelEditorPawn::OnMouseWheel);
        }
    }

    // Keyboard shortcuts (traditional BindKey, coexists with EnhancedInput)
    PlayerInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush1);
    PlayerInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush2);
    PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush3);
    PlayerInputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush4);
    PlayerInputComponent->BindKey(EKeys::Five,  IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush5);
    PlayerInputComponent->BindKey(EKeys::Six,   IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush6);
    PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush7);
    PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush8);
    PlayerInputComponent->BindKey(EKeys::E,     IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrushEraser);

    PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ALevelEditorPawn::HandleShortcutEsc);
    PlayerInputComponent->BindKey(EKeys::F5,     IE_Pressed, this, &ALevelEditorPawn::HandleShortcutTest);

    // Ctrl+key combos: use FInputChord via BindKey
    FInputKeyBinding& BindNew = PlayerInputComponent->KeyBindings.Add_GetRef(
        FInputKeyBinding(FInputChord(EKeys::N, false, true, false, false), IE_Pressed));
    BindNew.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALevelEditorPawn::HandleShortcutNew);

    FInputKeyBinding& BindSave = PlayerInputComponent->KeyBindings.Add_GetRef(
        FInputKeyBinding(FInputChord(EKeys::S, false, true, false, false), IE_Pressed));
    BindSave.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALevelEditorPawn::HandleShortcutSave);

    FInputKeyBinding& BindLoad = PlayerInputComponent->KeyBindings.Add_GetRef(
        FInputKeyBinding(FInputChord(EKeys::O, false, true, false, false), IE_Pressed));
    BindLoad.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ALevelEditorPawn::HandleShortcutLoad);
}

void ALevelEditorPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsPainting)
    {
        HandlePainting();
    }
    else if (bIsErasing)
    {
        HandleErasing();
    }

    if (bIsPanning)
    {
        HandlePanning(DeltaTime);
    }
}

bool ALevelEditorPawn::RaycastToGrid(FIntPoint& OutGridPos) const
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return false;

    FVector WorldOrigin, WorldDirection;
    if (!PC->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
    {
        return false;
    }

    // Intersect ray with Z=0 plane
    if (FMath::IsNearlyZero(WorldDirection.Z))
    {
        return false;
    }

    float T = -WorldOrigin.Z / WorldDirection.Z;
    if (T < 0.0f)
    {
        return false;
    }

    FVector HitPoint = WorldOrigin + T * WorldDirection;

    if (!GridManagerRef)
    {
        return false;
    }

    OutGridPos = GridManagerRef->WorldToGrid(HitPoint);
    return true;
}

void ALevelEditorPawn::OnLeftClickStarted(const FInputActionValue& Value)
{
    bIsPainting = true;
    bHasLastPaintedPos = false;
    HandlePainting();
}

void ALevelEditorPawn::OnLeftClickCompleted(const FInputActionValue& Value)
{
    bIsPainting = false;
    bHasLastPaintedPos = false;
}

void ALevelEditorPawn::OnRightClickStarted(const FInputActionValue& Value)
{
    // In plate placement mode, right-click cancels (returns to Normal)
    ALevelEditorGameMode* EditorGM = Cast<ALevelEditorGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));
    if (EditorGM && EditorGM->GetEditorMode() != EEditorMode::Normal)
    {
        EditorGM->CancelPlacementMode();
        return;
    }

    bIsErasing = true;
    bHasLastPaintedPos = false;
    HandleErasing();
}

void ALevelEditorPawn::OnRightClickCompleted(const FInputActionValue& Value)
{
    bIsErasing = false;
    bHasLastPaintedPos = false;
    bEraseConfirmPending = false;
    bHasLastErasedPos = false;
}

void ALevelEditorPawn::OnMiddleClickStarted(const FInputActionValue& Value)
{
    bIsPanning = true;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        float MouseX, MouseY;
        PC->GetMousePosition(MouseX, MouseY);
        PanStartMousePos = FVector2D(MouseX, MouseY);
        PanStartActorLocation = GetActorLocation();
    }
}

void ALevelEditorPawn::OnMiddleClickCompleted(const FInputActionValue& Value)
{
    bIsPanning = false;
}

void ALevelEditorPawn::OnMouseWheel(const FInputActionValue& Value)
{
    if (!EditorCamera) return;

    float ScrollValue = Value.Get<float>();
    float NewWidth = EditorCamera->OrthoWidth - ScrollValue * ZoomSpeed;
    EditorCamera->OrthoWidth = FMath::Clamp(NewWidth, MinOrthoWidth, MaxOrthoWidth);
}

void ALevelEditorPawn::HandlePainting()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    FIntPoint GridPos;
    if (!RaycastToGrid(GridPos)) return;

    if (bHasLastPaintedPos && LastPaintedGridPos == GridPos) return;

    LastPaintedGridPos = GridPos;
    bHasLastPaintedPos = true;

    ALevelEditorGameMode* EditorGM = Cast<ALevelEditorGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));
    if (EditorGM)
    {
        EditorGM->PaintAtGrid(GridPos);

        if (MainWidget) MainWidget->RefreshStats();
    }
}

void ALevelEditorPawn::HandleErasing()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    FIntPoint GridPos;
    if (!RaycastToGrid(GridPos)) return;

    ALevelEditorGameMode* EditorGM = Cast<ALevelEditorGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));
    if (!EditorGM) return;

    // Check if this cell requires erase confirmation (e.g. Door)
    if (EditorGM->ShouldConfirmErase(GridPos))
    {
        if (!bEraseConfirmPending)
        {
            bEraseConfirmPending = true;
            if (MainWidget)
            {
                MainWidget->RequestEraseConfirm(GridPos);
            }
        }
        return;
    }

    // Normal erase with dedup
    if (bHasLastErasedPos && LastErasedGridPos == GridPos) return;

    EditorGM->EraseAtGrid(GridPos);
    LastErasedGridPos = GridPos;
    bHasLastErasedPos = true;

    if (MainWidget) MainWidget->RefreshStats();
}

void ALevelEditorPawn::HandlePanning(float DeltaTime)
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !EditorCamera) return;

    float MouseX, MouseY;
    PC->GetMousePosition(MouseX, MouseY);
    FVector2D CurrentMousePos(MouseX, MouseY);

    FVector2D MouseDelta = CurrentMousePos - PanStartMousePos;

    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

    if (ViewportSizeX <= 0 || ViewportSizeY <= 0) return;

    float WorldUnitsPerPixel = EditorCamera->OrthoWidth / static_cast<float>(ViewportSizeX);

    FVector WorldDelta(
        -MouseDelta.X * WorldUnitsPerPixel,
        MouseDelta.Y * WorldUnitsPerPixel,
        0.0f);

    FVector NewLocation = PanStartActorLocation + WorldDelta;
    NewLocation.Z = GetActorLocation().Z;
    SetActorLocation(NewLocation);
}

// ===== Keyboard Shortcut Handlers =====

void ALevelEditorPawn::TrySetBrush(EEditorBrush Brush)
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    ALevelEditorGameMode* GM = Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode());
    if (GM && GM->GetEditorMode() != EEditorMode::Normal) return;

    if (MainWidget) MainWidget->RequestSetBrush(Brush);
}

void ALevelEditorPawn::HandleKeyBrush1()     { TrySetBrush(EEditorBrush::Floor); }
void ALevelEditorPawn::HandleKeyBrush2()     { TrySetBrush(EEditorBrush::Wall); }
void ALevelEditorPawn::HandleKeyBrush3()     { TrySetBrush(EEditorBrush::Ice); }
void ALevelEditorPawn::HandleKeyBrush4()     { TrySetBrush(EEditorBrush::Goal); }
void ALevelEditorPawn::HandleKeyBrush5()     { TrySetBrush(EEditorBrush::Door); }
void ALevelEditorPawn::HandleKeyBrush6()     { TrySetBrush(EEditorBrush::PressurePlate); }
void ALevelEditorPawn::HandleKeyBrush7()     { TrySetBrush(EEditorBrush::BoxSpawn); }
void ALevelEditorPawn::HandleKeyBrush8()     { TrySetBrush(EEditorBrush::PlayerStart); }
void ALevelEditorPawn::HandleKeyBrushEraser(){ TrySetBrush(EEditorBrush::Eraser); }

void ALevelEditorPawn::HandleShortcutNew()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;
    if (MainWidget) MainWidget->RequestToolbarAction(EToolbarAction::New);
}

void ALevelEditorPawn::HandleShortcutSave()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;
    if (MainWidget) MainWidget->RequestToolbarAction(EToolbarAction::Save);
}

void ALevelEditorPawn::HandleShortcutLoad()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;
    if (MainWidget) MainWidget->RequestToolbarAction(EToolbarAction::Load);
}

void ALevelEditorPawn::HandleShortcutTest()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;
    if (MainWidget) MainWidget->RequestToolbarAction(EToolbarAction::Test);
}

void ALevelEditorPawn::HandleShortcutEsc()
{
    if (MainWidget)
    {
        MainWidget->HandleEscPressed();
    }
}
