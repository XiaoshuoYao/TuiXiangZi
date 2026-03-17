#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TimelineComponent.h"
#include "Grid/GridTypes.h"
#include "SokobanCharacter.generated.h"

class AGridManager;
class UCurveFloat;
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
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    FIntPoint CurrentGridPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bIsMoving = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MoveDuration = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    UCurveFloat* MoveCurve;

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

    void OnMoveUp(const struct FInputActionValue& Value);
    void OnMoveDown(const struct FInputActionValue& Value);
    void OnMoveLeft(const struct FInputActionValue& Value);
    void OnMoveRight(const struct FInputActionValue& Value);
    void OnMoveInput(EMoveDirection Dir);

    // ===== 平滑移动 =====
    UPROPERTY(VisibleAnywhere, Category = "Movement")
    UTimelineComponent* MoveTimeline;

    FVector MoveStartLocation;
    FVector MoveTargetLocation;

    void SmoothMoveTo(FVector TargetWorldPos);

    UFUNCTION()
    void OnMoveTimelineUpdate(float Alpha);

    UFUNCTION()
    void OnMoveTimelineFinished();

    void OnMoveCompleted();

    void OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To);

    // ===== 相机 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* TopDownCamera;

    // ===== 引用 =====
    UPROPERTY()
    AGridManager* GridManagerRef;
};
