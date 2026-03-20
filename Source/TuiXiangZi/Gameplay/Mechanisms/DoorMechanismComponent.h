#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "DoorMechanismComponent.generated.h"

class UTimelineComponent;
class UCurveFloat;

UCLASS(Blueprintable, ClassGroup = "GridMechanism",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UDoorMechanismComponent : public UGridMechanismComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
	virtual bool BlocksPassage() const override { return true; }
	virtual bool IsCurrentlyBlocking() const override { return !bIsOpen; }

	void SetDoorOpen(bool bOpen);
	void SetDoorStateImmediate(bool bOpen);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsOpen = false;

protected:
	UPROPERTY()
	UTimelineComponent* DoorTimeline = nullptr;

	UPROPERTY(EditAnywhere, Category = "Door")
	UCurveFloat* DoorCurve;

	float DoorClosedZ = 0.0f;
	float DoorOpenZ = 0.0f;

	UFUNCTION()
	void OnDoorTimelineUpdate(float Alpha);

	UFUNCTION()
	void OnDoorTimelineFinished();
};
