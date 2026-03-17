#include "Gameplay/SokobanCharacter.h"
#include "Grid/GridManager.h"
#include "Framework/SokobanGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"

ASokobanCharacter::ASokobanCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    // 禁用 CharacterMovement 自由移动
    GetCharacterMovement()->MaxWalkSpeed = 0.0f;
    GetCharacterMovement()->GravityScale = 0.0f;

    // 相机 SpringArm - 俯视角
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetRelativeRotation(FRotator(-75.0f, 0.0f, 0.0f));
    CameraBoom->TargetArmLength = 1200.0f;
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;

    // 相机
    TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

    // Timeline
    MoveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("MoveTimeline"));
}

void ASokobanCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 查找 GridManager
    GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    // 添加 Enhanced Input Mapping Context
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (SokobanMappingContext)
            {
                Subsystem->AddMappingContext(SokobanMappingContext, 0);
            }
        }
    }

    // 设置 Timeline
    if (MoveCurve)
    {
        FOnTimelineFloat UpdateDelegate;
        UpdateDelegate.BindUFunction(this, FName("OnMoveTimelineUpdate"));
        MoveTimeline->AddInterpFloat(MoveCurve, UpdateDelegate);

        FOnTimelineEvent FinishedDelegate;
        FinishedDelegate.BindUFunction(this, FName("OnMoveTimelineFinished"));
        MoveTimeline->SetTimelineFinishedFunc(FinishedDelegate);

        MoveTimeline->SetPlayRate(1.0f / MoveDuration);
    }

    // 绑定 GridManager 广播
    if (GridManagerRef)
    {
        GridManagerRef->OnActorLogicalMoved.AddUObject(this, &ASokobanCharacter::OnActorLogicalMoved);
    }
}

void ASokobanCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveUpAction)
            EnhancedInput->BindAction(MoveUpAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveUp);
        if (MoveDownAction)
            EnhancedInput->BindAction(MoveDownAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveDown);
        if (MoveLeftAction)
            EnhancedInput->BindAction(MoveLeftAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveLeft);
        if (MoveRightAction)
            EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveRight);
    }
}

void ASokobanCharacter::OnMoveUp(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Up); }
void ASokobanCharacter::OnMoveDown(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Down); }
void ASokobanCharacter::OnMoveLeft(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Left); }
void ASokobanCharacter::OnMoveRight(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Right); }

void ASokobanCharacter::OnMoveInput(EMoveDirection Dir)
{
    // 移动锁：动画期间忽略输入
    if (bIsMoving) return;
    if (!GridManagerRef) return;

    // Capture snapshot before attempting move (for undo)
    ASokobanGameState* GS = GetWorld()->GetGameState<ASokobanGameState>();
    if (GS && GridManagerRef)
    {
        GS->PushSnapshot(GS->CaptureSnapshot(GridManagerRef));
    }

    // 尝试移动
    bool bSuccess = GridManagerRef->TryMoveActor(CurrentGridPos, Dir);
    // 移动成功时 GridManager 会广播 OnActorLogicalMoved，角色在回调中执行 SmoothMoveTo
    if (bSuccess)
    {
        if (GS)
        {
            GS->IncrementSteps();
        }
    }
    else
    {
        // Rollback snapshot for failed move
        if (GS)
        {
            GS->PopSnapshot();
        }
        // 可在此播放"撞墙"反馈（Phase 5）
    }
}

void ASokobanCharacter::SnapToGridPos(FIntPoint GridPos)
{
    CurrentGridPos = GridPos;
    if (GridManagerRef)
    {
        FVector WorldPos = GridManagerRef->GridToWorld(GridPos);
        WorldPos.Z = GetActorLocation().Z;
        SetActorLocation(WorldPos);
    }
}

void ASokobanCharacter::SmoothMoveTo(FVector TargetWorldPos)
{
    if (!MoveCurve)
    {
        // 无曲线时直接瞬移
        TargetWorldPos.Z = GetActorLocation().Z;
        SetActorLocation(TargetWorldPos);
        return;
    }

    bIsMoving = true;
    MoveStartLocation = GetActorLocation();
    MoveTargetLocation = TargetWorldPos;
    MoveTargetLocation.Z = MoveStartLocation.Z; // 保持 Z 不变

    MoveTimeline->PlayFromStart();
}

void ASokobanCharacter::OnMoveTimelineUpdate(float Alpha)
{
    FVector NewLocation = FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha);
    SetActorLocation(NewLocation);
}

void ASokobanCharacter::OnMoveTimelineFinished()
{
    OnMoveCompleted();
}

void ASokobanCharacter::OnMoveCompleted()
{
    bIsMoving = false;
    // 精确对齐格子中心
    if (GridManagerRef)
    {
        FVector SnapPos = GridManagerRef->GridToWorld(CurrentGridPos);
        SnapPos.Z = GetActorLocation().Z;
        SetActorLocation(SnapPos);
    }
}

void ASokobanCharacter::OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To)
{
    // 只响应自身的移动事件
    if (Actor != this) return;

    CurrentGridPos = To;
    if (GridManagerRef)
    {
        SmoothMoveTo(GridManagerRef->GridToWorld(To));
    }
}
