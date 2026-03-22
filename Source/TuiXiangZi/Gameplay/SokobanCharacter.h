#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Grid/GridTypes.h"
#include "Events/GameEventPayload.h"
#include "SokobanCharacter.generated.h"

class AGridManager;
class UGameEventBus;
class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;

UCLASS(Blueprintable)
class TUIXIANGZI_API ASokobanCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASokobanCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    FIntPoint CurrentGridPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bIsMoving = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bIsSliding = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MoveDuration = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float IceSlideSpeedMultiplier = 2.0f;

    void SnapToGridPos(FIntPoint GridPos);

protected:
    // ===== Enhanced Input =====
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* SokobanMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveUpAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveDownAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveLeftAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveRightAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* UndoAction;

    void OnMoveUp(const struct FInputActionValue& Value);
    void OnMoveDown(const struct FInputActionValue& Value);
    void OnMoveLeft(const struct FInputActionValue& Value);
    void OnMoveRight(const struct FInputActionValue& Value);
    void OnMoveReleased(const struct FInputActionValue& Value);
    void OnMoveInput(EMoveDirection Dir);
    void OnUndo(const struct FInputActionValue& Value);

    // ===== 持续移动 =====
    EMoveDirection HeldDirection = EMoveDirection::Up;
    bool bHoldingDirection = false;

    // ===== 网格移动 =====
    FVector MoveStartLocation;
    FVector MoveTargetLocation;
    FVector MoveDirection;
    float SlideElapsed = 0.0f;
    float SlideTotalDuration = 0.0f;

    void OnActorMovedEvent(FName EventTag, const FGameEventPayload& Payload);
    void OnTeleportedEvent(FName EventTag, const FGameEventPayload& Payload);

    // ===== 相机 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* TopDownCamera;

    // ===== 引用 =====
    UPROPERTY()
    AGridManager* GridManagerRef;
};
