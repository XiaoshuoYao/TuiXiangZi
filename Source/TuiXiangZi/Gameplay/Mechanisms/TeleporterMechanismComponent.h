#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "TeleporterMechanismComponent.generated.h"

/**
 * Teleporter mechanism — paired one-to-one via the group system.
 * ExtraParam encoding (stored on FGridCell):
 *   0 = Bidirectional  (sends and receives)
 *   1 = Entry          (sends only — stepping here teleports you away)
 *   2 = Exit           (receives only — destination, stepping here does nothing)
 */
UCLASS(Blueprintable, ClassGroup = "GridMechanism",
	meta = (BlueprintSpawnableComponent))
class TUIXIANGZI_API UTeleporterMechanismComponent : public UGridMechanismComponent
{
	GENERATED_BODY()

public:
	virtual EEditorPlacementFlow GetEditorPlacementFlow() const override { return EEditorPlacementFlow::PairPlacement; }

	/** Whether an actor stepping on this teleporter should be teleported away. */
	bool CanSend() const;

	/** Whether this teleporter can receive an incoming teleport. */
	bool CanReceive() const;

	/** Set the role from ExtraParam (0=Bidirectional, 1=Entry, 2=Exit). */
	void SetRoleFromExtraParam(int32 ExtraParam);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleporter")
	int32 Role = 0; // 0=Bidirectional, 1=Entry, 2=Exit
};
