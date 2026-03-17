#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Mechanisms/GridMechanism.h"
#include "PressurePlate.generated.h"

UCLASS()
class TUIXIANGZI_API APressurePlate : public AGridMechanism
{
	GENERATED_BODY()

public:
	APressurePlate();

	virtual void OnActivate() override;
	virtual void OnDeactivate() override;
};
