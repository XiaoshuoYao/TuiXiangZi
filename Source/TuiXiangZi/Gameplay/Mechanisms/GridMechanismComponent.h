#pragma once

#include "CoreMinimal.h"
#include "Gameplay/GridTileComponent.h"
#include "GridMechanismComponent.generated.h"

UCLASS(Abstract, Blueprintable, ClassGroup = "GridMechanism",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UGridMechanismComponent : public UGridTileComponent
{
	GENERATED_BODY()

public:
	// ---- Activation interface ----
	virtual void OnActivate();
	virtual void OnDeactivate();
	bool IsActivated() const { return bIsActivated; }

	// ---- Passability ----
	virtual bool BlocksPassage() const { return false; }
	virtual bool IsCurrentlyBlocking() const { return false; }

	// ---- Group Role ----
	/** Whether this mechanism acts as a trigger in the group system (e.g. pressure plate). */
	virtual bool IsGroupTrigger() const { return false; }

	// ---- Color system (activation-aware override) ----
	virtual void SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor) override;

protected:
	bool bIsActivated = false;
};
