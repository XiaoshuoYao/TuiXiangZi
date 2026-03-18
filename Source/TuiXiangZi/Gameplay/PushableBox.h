#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "PushableBox.generated.h"

class AGridManager;
class UCurveFloat;

UCLASS(Blueprintable)
class TUIXIANGZI_API APushableBox : public AActor
{
    GENERATED_BODY()

public:
    APushableBox();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    FIntPoint CurrentGridPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    UStaticMeshComponent* MeshComp;

    UPROPERTY()
    UMaterialInstanceDynamic* DynamicMaterialInst;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    bool bIsMoving = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MoveDuration = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    UCurveFloat* MoveCurve;

    void SnapToGridPos(FIntPoint GridPos);
    void SmoothMoveTo(FVector TargetWorldPos);
    void PlayFallIntoHoleAnim();

    /** Visual feedback: box on pressure plate glow. */
    void SetOnPlateVisual(bool bOnPlate, FLinearColor GroupColor);

protected:
    UPROPERTY(VisibleAnywhere, Category = "Movement")
    UTimelineComponent* MoveTimeline;

    FVector MoveStartLocation;
    FVector MoveTargetLocation;

    UFUNCTION()
    void OnMoveTimelineUpdate(float Alpha);

    UFUNCTION()
    void OnMoveTimelineFinished();

    void OnMoveCompleted();

    void OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To);

    UPROPERTY()
    AGridManager* GridManagerRef;
};
