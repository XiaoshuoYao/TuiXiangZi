#include "Editor/LevelEditorPawn.h"
#include "Grid/GridManager.h"
#include "Editor/LevelEditorGameMode.h"
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
    bIsErasing = true;
    bHasLastPaintedPos = false;
    HandleErasing();
}

void ALevelEditorPawn::OnRightClickCompleted(const FInputActionValue& Value)
{
    bIsErasing = false;
    bHasLastPaintedPos = false;
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
    }
}

void ALevelEditorPawn::HandleErasing()
{
    FIntPoint GridPos;
    if (!RaycastToGrid(GridPos)) return;

    if (bHasLastPaintedPos && LastPaintedGridPos == GridPos) return;

    LastPaintedGridPos = GridPos;
    bHasLastPaintedPos = true;

    ALevelEditorGameMode* EditorGM = Cast<ALevelEditorGameMode>(
        UGameplayStatics::GetGameMode(GetWorld()));
    if (EditorGM)
    {
        EditorGM->EraseAtGrid(GridPos);
    }
}

void ALevelEditorPawn::HandlePanning(float DeltaTime)
{
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
