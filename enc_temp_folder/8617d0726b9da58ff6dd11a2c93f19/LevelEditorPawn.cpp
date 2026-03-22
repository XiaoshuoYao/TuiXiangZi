#include "Editor/LevelEditorPawn.h"
#include "Grid/GridManager.h"
#include "Editor/LevelEditorGameMode.h"
#include "Editor/EditorGridVisualizer.h"
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

    // Perspective camera as root
    EditorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("EditorCamera"));
    RootComponent = EditorCamera;

    EditorCamera->ProjectionMode = ECameraProjectionMode::Perspective;
    EditorCamera->FieldOfView = 60.0f;
    EditorCamera->SetRelativeRotation(FRotator(-50.0f, 0.0f, 0.0f));
    SetActorLocation(FVector(0.0f, 0.0f, 1000.0f));

    AutoPossessAI = EAutoPossessAI::Disabled;
}

void ALevelEditorPawn::BeginPlay()
{
    Super::BeginPlay();

    SetActorLocation(FVector(400.0f, -500.0f, 800.0f));

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

void ALevelEditorPawn::FocusCameraOnGrid()
{
    if (!GridManagerRef)
    {
        GridManagerRef = Cast<AGridManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
    }
    if (!GridManagerRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("FocusCameraOnGrid: GridManagerRef is null!"));
        return;
    }

    FIntRect Bounds = GridManagerRef->GetGridBounds();
    UE_LOG(LogTemp, Log, TEXT("FocusCameraOnGrid: GridBounds Min=(%d,%d) Max=(%d,%d)"),
        Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y);

    // Compute grid center in world space
    FVector MinWorld = GridManagerRef->GridToWorld(Bounds.Min);
    FVector MaxWorld = GridManagerRef->GridToWorld(FIntPoint(Bounds.Max.X - 1, Bounds.Max.Y - 1));
    FVector GridCenter = (MinWorld + MaxWorld) * 0.5f;
    GridCenter.Z = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("FocusCameraOnGrid: GridCenter=(%.1f, %.1f, %.1f)"),
        GridCenter.X, GridCenter.Y, GridCenter.Z);

    // Choose camera height based on grid extent
    float GridExtent = FMath::Max(MaxWorld.X - MinWorld.X, MaxWorld.Y - MinWorld.Y)
                       + GridManagerRef->CellSize;
    float CameraHeight = FMath::Max(GridExtent * 0.8f, 400.0f);

    // Set camera rotation to default pitch + yaw 90 (camera looks along +Y)
    FRotator CamRot(-50.0f, 90.0f, 0.0f);
    SetActorRotation(CamRot);

    float PitchRad = FMath::DegreesToRadians(50.0f);
    float Distance = CameraHeight / FMath::Sin(PitchRad);
    FVector CamPos = GridCenter - CamRot.Vector() * Distance;

    UE_LOG(LogTemp, Log, TEXT("FocusCameraOnGrid: CamRot=(P=%.1f, Y=%.1f, R=%.1f) Height=%.1f Distance=%.1f"),
        CamRot.Pitch, CamRot.Yaw, CamRot.Roll, CameraHeight, Distance);
    UE_LOG(LogTemp, Log, TEXT("FocusCameraOnGrid: Setting camera to (%.1f, %.1f, %.1f)"),
        CamPos.X, CamPos.Y, CamPos.Z);

    SetActorLocation(CamPos);
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
    PlayerInputComponent->BindKey(EKeys::Nine,   IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrush9);
    PlayerInputComponent->BindKey(EKeys::Zero,   IE_Pressed, this, &ALevelEditorPawn::HandleKeyBrushEraser);

    PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ALevelEditorPawn::HandleShortcutEsc);
    PlayerInputComponent->BindKey(EKeys::F5,     IE_Pressed, this, &ALevelEditorPawn::HandleShortcutTest);
    PlayerInputComponent->BindKey(EKeys::G,      IE_Pressed, this, &ALevelEditorPawn::HandleToggleCoordinateLabels);

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

    if (bIsRotating)
    {
        HandleRotation();
    }

    HandleKeyboardMovement(DeltaTime);
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
    bIsRotating = true;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        float MouseX, MouseY;
        PC->GetMousePosition(MouseX, MouseY);
        LastMousePos = FVector2D(MouseX, MouseY);
    }
}

void ALevelEditorPawn::OnMiddleClickCompleted(const FInputActionValue& Value)
{
    bIsRotating = false;
}

void ALevelEditorPawn::OnMouseWheel(const FInputActionValue& Value)
{
    if (!EditorCamera) return;

    float ScrollValue = Value.Get<float>();
    FVector Forward = EditorCamera->GetForwardVector();
    FVector NewLocation = GetActorLocation() + Forward * ScrollValue * ZoomSpeed;
    // Prevent camera from going below ground
    if (NewLocation.Z >= 50.0f)
    {
        SetActorLocation(NewLocation);
    }
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

void ALevelEditorPawn::HandleRotation()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    float MouseX, MouseY;
    PC->GetMousePosition(MouseX, MouseY);
    FVector2D CurrentMouse(MouseX, MouseY);

    FVector2D Delta = CurrentMouse - LastMousePos;
    LastMousePos = CurrentMouse;

    FRotator NewRot = GetActorRotation();
    NewRot.Yaw += Delta.X * MouseSensitivity;
    NewRot.Pitch = FMath::Clamp(NewRot.Pitch - Delta.Y * MouseSensitivity, -89.0f, 89.0f);
    SetActorRotation(NewRot);
}

void ALevelEditorPawn::HandleKeyboardMovement(float DeltaTime)
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // Get camera yaw for horizontal movement direction
    FRotator YawRotation(0.0f, GetActorRotation().Yaw, 0.0f);
    FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    FVector MoveDirection = FVector::ZeroVector;

    if (PC->IsInputKeyDown(EKeys::W)) MoveDirection += ForwardDir;
    if (PC->IsInputKeyDown(EKeys::S)) MoveDirection -= ForwardDir;
    if (PC->IsInputKeyDown(EKeys::D)) MoveDirection += RightDir;
    if (PC->IsInputKeyDown(EKeys::A)) MoveDirection -= RightDir;
    if (PC->IsInputKeyDown(EKeys::E)) MoveDirection += FVector::UpVector;
    if (PC->IsInputKeyDown(EKeys::Q)) MoveDirection -= FVector::UpVector;

    if (!MoveDirection.IsNearlyZero())
    {
        MoveDirection.Normalize();
        FVector NewLocation = GetActorLocation() + MoveDirection * MoveSpeed * DeltaTime;
        // Prevent going below ground
        if (NewLocation.Z < 50.0f) NewLocation.Z = 50.0f;
        SetActorLocation(NewLocation);
    }
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

void ALevelEditorPawn::HandleToggleCoordinateLabels()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    ALevelEditorGameMode* EditorGM = Cast<ALevelEditorGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));
    if (!EditorGM) return;

    // Find the EditorGridVisualizer in the world
    AEditorGridVisualizer* Visualizer = Cast<AEditorGridVisualizer>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AEditorGridVisualizer::StaticClass()));
    if (Visualizer)
    {
        Visualizer->ToggleCoordinateLabels();
    }
}
