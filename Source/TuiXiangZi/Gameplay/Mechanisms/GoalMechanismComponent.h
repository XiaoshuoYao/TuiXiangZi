#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "GoalMechanismComponent.generated.h"

UCLASS(Blueprintable, ClassGroup = "GridMechanism",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UGoalMechanismComponent : public UGridMechanismComponent
{
	GENERATED_BODY()

public:
	virtual bool BlocksPassage() const override { return false; }
};
