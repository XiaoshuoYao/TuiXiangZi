#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "PressurePlateMechanismComponent.generated.h"

UCLASS(Blueprintable, ClassGroup = "GridMechanism",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UPressurePlateMechanismComponent : public UGridMechanismComponent
{
	GENERATED_BODY()

public:
	virtual bool IsGroupTrigger() const override { return true; }
	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
};
