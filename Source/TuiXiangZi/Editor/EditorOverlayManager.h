#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EditorOverlayManager.generated.h"

class UEditorOverlayComponent;
class UGridLineOverlay;
class UCoordinateLabelOverlay;
class UTeleporterArrowOverlay;

/** Grid overlay display mode, cycled by pressing G. */
UENUM(BlueprintType)
enum class EGridOverlayMode : uint8
{
	None,
	GridLines,
	Coordinates,
	Both
};

/**
 * Manages editor overlay components. Replaces the old EditorGridVisualizer.
 * Owns overlay components that independently subscribe to GameEventBus events.
 * Handles overlay mode cycling and coordinated debug line flush/rebuild.
 */
UCLASS(Blueprintable)
class TUIXIANGZI_API AEditorOverlayManager : public AActor
{
	GENERATED_BODY()

public:
	AEditorOverlayManager();
	virtual void BeginPlay() override;

	/** Cycle overlay mode: None → GridLines → Coordinates → Both → None. */
	UFUNCTION(BlueprintCallable, Category = "Editor|Overlay")
	void CycleOverlayMode();

	UFUNCTION(BlueprintCallable, Category = "Editor|Overlay")
	EGridOverlayMode GetOverlayMode() const { return OverlayMode; }

	/**
	 * Flush all persistent debug lines, then ask all visible debug-line overlays
	 * to redraw. Called by overlay components via RequestDebugLineFlush().
	 */
	void FlushAndRebuildDebugLines();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Editor|Overlay")
	UGridLineOverlay* GridLineOverlay;

	UPROPERTY(VisibleAnywhere, Category = "Editor|Overlay")
	UCoordinateLabelOverlay* CoordinateLabelOverlay;

	UPROPERTY(VisibleAnywhere, Category = "Editor|Overlay")
	UTeleporterArrowOverlay* TeleporterArrowOverlay;

	EGridOverlayMode OverlayMode = EGridOverlayMode::None;

	void ApplyOverlayMode();

	/** Collect all overlay components on this actor. */
	TArray<UEditorOverlayComponent*> GetAllOverlays() const;
};
