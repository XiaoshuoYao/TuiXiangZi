#pragma once

#include "CoreMinimal.h"
#include "Editor/EditorOverlayComponent.h"
#include "CoordinateLabelOverlay.generated.h"

class ATextRenderActor;

/**
 * Overlay that spawns coordinate label TextRenderActors on each grid cell.
 * Subscribes to Editor.GridBoundsChanged.
 */
UCLASS()
class TUIXIANGZI_API UCoordinateLabelOverlay : public UEditorOverlayComponent
{
	GENERATED_BODY()

public:
	virtual void Rebuild() override;
	virtual void Clear() override;
	virtual bool IsModeCycled() const override { return true; }

protected:
	virtual void SubscribeToEvents(UGameEventBus* EventBus) override;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	int32 PaddingCells = 2;

private:
	FIntPoint CachedMin = FIntPoint::ZeroValue;
	FIntPoint CachedMax = FIntPoint::ZeroValue;
	float CachedCellSize = 100.0f;
	bool bHasData = false;

	UPROPERTY()
	TArray<ATextRenderActor*> LabelActors;

	void OnGridBoundsChanged(FName EventTag, const FGameEventPayload& Payload);
};
