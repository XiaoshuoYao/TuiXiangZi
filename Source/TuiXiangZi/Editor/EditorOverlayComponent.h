#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Events/GameEventPayload.h"
#include "EditorOverlayComponent.generated.h"

class UGameEventBus;
class AEditorOverlayManager;

/**
 * Abstract base class for editor overlay components.
 * Each overlay subscribes to GameEventBus events and manages its own visualization.
 */
UCLASS(Abstract)
class TUIXIANGZI_API UEditorOverlayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Core interface ----

	/** Full rebuild: clear + redraw from cached data. */
	virtual void Rebuild() PURE_VIRTUAL(UEditorOverlayComponent::Rebuild, );

	/** Clear all visuals. */
	virtual void Clear() PURE_VIRTUAL(UEditorOverlayComponent::Clear, );

	/**
	 * Redraw debug lines only (no flush). Called by Manager after a global
	 * FlushPersistentDebugLines to restore this overlay's lines.
	 * Override only if this overlay uses DrawDebugLine.
	 */
	virtual void RedrawDebugLines() {}

	/** Whether this overlay uses DrawDebugLine (for flush coordination). */
	virtual bool UsesDebugLines() const { return false; }

	void SetVisible(bool bNewVisible);
	bool IsVisible() const { return bIsVisible; }

	/** Whether this overlay participates in overlay mode cycling (G key). Default true. */
	virtual bool IsModeCycled() const { return true; }

protected:
	bool bIsVisible = false;
	bool bDataDirty = false;

	/** Subclasses override to subscribe to events. */
	virtual void SubscribeToEvents(UGameEventBus* EventBus) {}

	/** Request a coordinated flush+rebuild of all debug-line overlays via the Manager. */
	void RequestDebugLineFlush();

	/** Get the owning overlay manager. */
	AEditorOverlayManager* GetOverlayManager() const;
};
