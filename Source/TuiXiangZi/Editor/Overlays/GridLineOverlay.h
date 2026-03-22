#pragma once

#include "CoreMinimal.h"
#include "Editor/EditorOverlayComponent.h"
#include "GridLineOverlay.generated.h"

/**
 * Overlay that draws grid border lines using DrawDebugLine.
 * Subscribes to Editor.GridBoundsChanged.
 */
UCLASS()
class TUIXIANGZI_API UGridLineOverlay : public UEditorOverlayComponent
{
	GENERATED_BODY()

public:
	virtual void Rebuild() override;
	virtual void Clear() override;
	virtual void RedrawDebugLines() override;
	virtual bool UsesDebugLines() const override { return true; }
	virtual bool IsModeCycled() const override { return true; }

protected:
	virtual void SubscribeToEvents(UGameEventBus* EventBus) override;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	int32 PaddingCells = 2;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	float LineThickness = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	float LineZ = 0.0f;

private:
	FIntPoint CachedMin = FIntPoint::ZeroValue;
	FIntPoint CachedMax = FIntPoint::ZeroValue;
	float CachedCellSize = 100.0f;
	bool bHasData = false;

	void OnGridBoundsChanged(FName EventTag, const FGameEventPayload& Payload);
	void DrawLines();
};
