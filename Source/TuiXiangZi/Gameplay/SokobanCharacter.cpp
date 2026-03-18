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

        // 绑定 GridManager 广播
        GridManagerRef->OnActorLogicalMoved.AddUObject(this, &ASokobanCharacter::OnActorLogicalMoved);
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
    }
}

void ASokobanCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsMoving) return;

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
        // Update box-on-plate visuals and broadcast step count
        if (ASokobanGameMode* GM = Cast<ASokobanGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GM->UpdateBoxOnPlateVisuals();
            GM->OnStepCountChanged.Broadcast(GS ? GS->GetStepCount() : 0);
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
    if (!GridManagerRef)
    {
        GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
    }
    if (GridManagerRef)
    {
        FVector WorldPos = GridManagerRef->GridToWorld(GridPos);
        float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        WorldPos.Z = HalfHeight;
        TeleportTo(WorldPos, GetActorRotation(), false, true);
    }
}

void ASokobanCharacter::OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To)
{
    if (Actor != this) return;

    CurrentGridPos = To;
    if (GridManagerRef)
    {
        MoveTargetLocation = GridManagerRef->GridToWorld(To);
        MoveTargetLocation.Z = GetActorLocation().Z;
        MoveDirection = (MoveTargetLocation - GetActorLocation()).GetSafeNormal2D();
        bIsMoving = true;
    }
}
