#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PushableBoxComponent.generated.h"

class AGridManager;
class UTimelineComponent;
class UCurveFloat;
class UStaticMeshComponent;

UCLASS(Blueprintable, ClassGroup = "Gameplay", meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UPushableBoxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	FIntPoint CurrentGridPos;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
	FName VisualStyleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	UCurveFloat* MoveCurve;

	void SnapToGridPos(FIntPoint GridPos);
	void SmoothMoveTo(FVector TargetWorldPos);
	void PlayFallIntoHoleAnim();
	void SetOnPlateVisual(bool bOnPlate, FLinearColor GroupColor);

protected:
	UPROPERTY()
	UTimelineComponent* MoveTimeline = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterialInst = nullptr;

	UPROPERTY()
	AGridManager* GridManagerRef = nullptr;

	FVector MoveStartLocation;
	FVector MoveTargetLocation;

	UStaticMeshComponent* FindOwnerMeshComp() const;

	UFUNCTION()
	void OnMoveTimelineUpdate(float Alpha);

	UFUNCTION()
	void OnMoveTimelineFinished();

	void OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To);
};
