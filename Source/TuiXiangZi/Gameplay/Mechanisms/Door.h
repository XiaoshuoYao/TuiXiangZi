#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Mechanisms/GridMechanism.h"
#include "Door.generated.h"

class UTimelineComponent;
class UCurveFloat;

UCLASS()
class TUIXIANGZI_API ADoor : public AGridMechanism
{
	GENERATED_BODY()

public:
	ADoor();

	virtual void BeginPlay() override;
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;

	void SetDoorOpen(bool bOpen);
	void SetDoorStateImmediate(bool bOpen);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsOpen = false;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Door")
	UTimelineComponent* DoorTimeline;

	UPROPERTY(EditAnywhere, Category = "Door")
	UCurveFloat* DoorCurve;

	float DoorClosedZ = 0.0f;
	float DoorOpenZ = 0.0f;

	UFUNCTION()
	void OnDoorTimelineUpdate(float Alpha);

	UFUNCTION()
	void OnDoorTimelineFinished();
};
