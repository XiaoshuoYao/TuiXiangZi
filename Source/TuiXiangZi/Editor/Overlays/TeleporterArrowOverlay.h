#pragma once

#include "CoreMinimal.h"
#include "Editor/EditorOverlayComponent.h"
#include "TeleporterArrowOverlay.generated.h"

/**
 * Data for drawing a teleporter connection arrow.
 */
USTRUCT(BlueprintType)
struct FTeleporterArrowData
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint PosA;

	UPROPERTY()
	FIntPoint PosB;

	UPROPERTY()
	int32 ExtraParamA = 0; // 0=Bidirectional, 1=Entry(A→B), 2=Exit(B→A)

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;
};

/**
 * Overlay that draws arrows between teleporter pairs using DrawDebugLine.
 * Subscribes to cell change events and self-queries GridManager for teleporter data.
 */
UCLASS()
class TUIXIANGZI_API UTeleporterArrowOverlay : public UEditorOverlayComponent
{
	GENERATED_BODY()

public:
	virtual void Rebuild() override;
	virtual void Clear() override;
	virtual void RedrawDebugLines() override;
	virtual bool UsesDebugLines() const override { return true; }
	virtual bool IsModeCycled() const override { return false; } // always visible

protected:
	virtual void SubscribeToEvents(UGameEventBus* EventBus) override;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	float ArrowThickness = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	float ArrowHeadSize = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Overlay")
	float ArrowZ = 50.0f;

private:
	float CachedCellSize = 100.0f;
	TArray<FTeleporterArrowData> CachedArrows;

	void OnCellChanged(FName EventTag, const FGameEventPayload& Payload);
	void OnGridBoundsChanged(FName EventTag, const FGameEventPayload& Payload);

	/** Scan GridManager for all teleporter pairs and rebuild CachedArrows. */
	void CollectTeleporterData();

	void DrawArrows();
};
