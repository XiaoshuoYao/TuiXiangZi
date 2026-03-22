#include "Gameplay/SokobanCharacter.h"
#include "Grid/GridManager.h"
#include "Framework/SokobanGameMode.h"
#include "Framework/SokobanGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Events/GameEventBus.h"
#include "Events/GameEventTags.h"
#include "Gameplay/Modifiers/TileModifierComponent.h"

ASokobanCharacter::ASokobanCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // CMC 配置：不用重力，瞬间加速/刹车
    UCharacterMovementComponent* CMC = GetCharacterMovement();
    CMC->GravityScale = 0.0f;
    CMC->MaxAcceleration = 999999.0f;
    CMC->BrakingDecelerationWalking = 999999.0f;
    CMC->bOrientRotationToMovement = false;

    // 允许直接设置 Pawn 旋转，不被 Controller 覆盖
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

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
}

void ASokobanCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 强制设为行走模式，让 ABP_Manny 知道角色在地面上
    GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

    // 查找 GridManager
    GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    if (GridManagerRef)
    {
        // 根据格子大小和移动时间计算行走速度
        float CellSize = GridManagerRef->CellSize;
        GetCharacterMovement()->MaxWalkSpeed = CellSize / FMath::Max(MoveDuration, 0.01f);

        // Subscribe to ActorMoved via EventBus
        if (UGameEventBus* EventBus = GetWorld()->GetSubsystem<UGameEventBus>())
        {
            EventBus->Subscribe(GameEventTags::ActorMoved, FOnGameEvent::FDelegate::CreateUObject(this, &ASokobanCharacter::OnActorMovedEvent));
            EventBus->Subscribe(GameEventTags::Teleported, FOnGameEvent::FDelegate::CreateUObject(this, &ASokobanCharacter::OnTeleportedEvent));
        }
    }

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

    // 在 BeginPlay 中绑定 InputAction（SetupPlayerInputComponent 时 BP 属性可能还未反序列化）
    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (MoveUpAction)
            EnhancedInput->BindAction(MoveUpAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveUp);
        if (MoveDownAction)
            EnhancedInput->BindAction(MoveDownAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveDown);
        if (MoveLeftAction)
            EnhancedInput->BindAction(MoveLeftAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveLeft);
        if (MoveRightAction)
            EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnMoveRight);
        if (UndoAction)
            EnhancedInput->BindAction(UndoAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnUndo);
    }
}

void ASokobanCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsMoving) return;

    if (bIsSliding)
    {
        // 冰面滑行：直接插值位置，不驱动 CMC（不播放走路动画）
        SlideElapsed += DeltaTime;
        float Alpha = FMath::Clamp(SlideElapsed / FMath::Max(SlideTotalDuration, 0.001f), 0.0f, 1.0f);
        FVector NewPos = FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha);
        NewPos.Z = GetActorLocation().Z;
        SetActorLocation(NewPos);

        if (Alpha >= 1.0f)
        {
            bIsMoving = false;
            bIsSliding = false;
            GetCharacterMovement()->StopMovementImmediately();
        }
    }
    else
    {
        float Dist = FVector::DistXY(GetActorLocation(), MoveTargetLocation);
        const float SnapThreshold = 5.0f;

        if (Dist < SnapThreshold)
        {
            // 到达：精确吸附到格子中心
            FVector SnapPos = MoveTargetLocation;
            SnapPos.Z = GetActorLocation().Z;
            SetActorLocation(SnapPos);
            bIsMoving = false;
            GetCharacterMovement()->StopMovementImmediately();
        }
        else
        {
            // 驱动 CMC 向目标移动，ABP_Manny 自动获得速度和加速度
            AddMovementInput(MoveDirection, 1.0f);
        }
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
        if (UndoAction)
            EnhancedInput->BindAction(UndoAction, ETriggerEvent::Started, this, &ASokobanCharacter::OnUndo);
    }
}

void ASokobanCharacter::OnMoveUp(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Up); }
void ASokobanCharacter::OnMoveDown(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Down); }
void ASokobanCharacter::OnMoveLeft(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Left); }
void ASokobanCharacter::OnMoveRight(const FInputActionValue& Value) { OnMoveInput(EMoveDirection::Right); }

void ASokobanCharacter::OnUndo(const FInputActionValue& Value)
{
    if (bIsMoving) return;

    if (ASokobanGameMode* GM = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->IsPauseMenuVisible()) return;
        GM->RequestUndo();
        GM->UpdateBoxOnPlateVisuals();
    }
}

void ASokobanCharacter::OnMoveInput(EMoveDirection Dir)
{
    // 移动锁：动画期间忽略输入
    if (bIsMoving) return;
    if (!GridManagerRef) return;

    // Block movement when pause menu or level complete menu is visible
    if (ASokobanGameMode* GM = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode()))
    {
        if (GM->IsPauseMenuVisible()) return;
    }

    // Capture snapshot before attempting move (for undo)
    ASokobanGameState* GS = GetWorld()->GetGameState<ASokobanGameState>();
    if (GS && GridManagerRef)
    {
        GS->PushSnapshot(GS->CaptureSnapshot(GridManagerRef));
    }

    // 根据移动方向转身
    {
        float TargetYaw = 0.0f;
        switch (Dir)
        {
        case EMoveDirection::Up:    TargetYaw = 0.0f;   break;
        case EMoveDirection::Down:  TargetYaw = 180.0f; break;
        case EMoveDirection::Left:  TargetYaw = -90.0f;  break;
        case EMoveDirection::Right: TargetYaw = 90.0f;  break;
        }
        SetActorRotation(FRotator(0.0f, TargetYaw, 0.0f));
    }

    // 尝试移动
    bool bSuccess = GridManagerRef->TryMoveActor(CurrentGridPos, Dir);
    if (bSuccess)
    {
        if (GS)
        {
            GS->IncrementSteps();
        }
        // Update box-on-plate visuals
        if (ASokobanGameMode* GM = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->UpdateBoxOnPlateVisuals();
        }

        // Broadcast events via EventBus
        if (UGameEventBus* EventBus = GetWorld()->GetSubsystem<UGameEventBus>())
        {
            int32 Steps = GS ? GS->GetStepCount() : 0;
            EventBus->Broadcast(GameEventTags::StepCountChanged, FGameEventPayload::MakeInt(Steps));
            EventBus->Broadcast(GameEventTags::PlayerMoved, FGameEventPayload::MakeGridPos(CurrentGridPos));
        }
    }
    else
    {
        // Rollback snapshot for failed move
        if (GS)
        {
            GS->PopSnapshot();
        }
    }
}

void ASokobanCharacter::SnapToGridPos(FIntPoint GridPos)
{
    CurrentGridPos = GridPos;

    // Reset movement state to prevent stale animation from previous level
    bIsMoving = false;
    bIsSliding = false;
    MoveDirection = FVector::ZeroVector;
    GetCharacterMovement()->StopMovementImmediately();

    if (!GridManagerRef)
    {
        GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
    }
    if (GridManagerRef)
    {
        FVector WorldPos = GridManagerRef->GridToWorld(GridPos);
        float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        WorldPos.Z = HalfHeight;
        MoveTargetLocation = WorldPos;
        TeleportTo(WorldPos, GetActorRotation(), false, true);
    }
}

void ASokobanCharacter::OnTeleportedEvent(FName EventTag, const FGameEventPayload& Payload)
{
    if (Payload.Actor.Get() != this) return;
    SnapToGridPos(Payload.ToPos);
}

void ASokobanCharacter::OnActorMovedEvent(FName EventTag, const FGameEventPayload& Payload)
{
    if (Payload.Actor.Get() != this) return;

    CurrentGridPos = Payload.ToPos;
    if (GridManagerRef)
    {
        MoveStartLocation = GetActorLocation();
        MoveTargetLocation = GridManagerRef->GridToWorld(Payload.ToPos);
        MoveTargetLocation.Z = GetActorLocation().Z;
        MoveDirection = (MoveTargetLocation - GetActorLocation()).GetSafeNormal2D();
        bIsMoving = true;

        // 检测冰面滑行：起始格和目标格都有冰面 Modifier 时才用滑行模式（排除离开冰面的情况）
        UTileModifierComponent* FromMod = GridManagerRef->GetModifierAt(Payload.FromPos);
        UTileModifierComponent* ToMod = GridManagerRef->GetModifierAt(Payload.ToPos);
        if (FromMod && FromMod->ShouldContinueMovement(EMoveDirection::Up)
            && ToMod && ToMod->ShouldContinueMovement(EMoveDirection::Up))
        {
            bIsSliding = true;
            float Dist = FVector::Dist2D(MoveStartLocation, MoveTargetLocation);
            float NormalSpeed = GridManagerRef->CellSize / FMath::Max(MoveDuration, 0.01f);
            float SlideSpeed = NormalSpeed * IceSlideSpeedMultiplier;
            SlideTotalDuration = Dist / FMath::Max(SlideSpeed, 1.0f);
            SlideElapsed = 0.0f;
            // 停止 CMC，避免走路动画
            GetCharacterMovement()->StopMovementImmediately();
        }
        else
        {
            bIsSliding = false;
        }
    }
}
