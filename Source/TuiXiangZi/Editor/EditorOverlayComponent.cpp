#include "Editor/EditorOverlayComponent.h"
#include "Editor/EditorOverlayManager.h"
#include "Events/GameEventBus.h"

void UEditorOverlayComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGameEventBus* EventBus = GetWorld()->GetSubsystem<UGameEventBus>())
	{
		SubscribeToEvents(EventBus);
	}
}

void UEditorOverlayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();

	if (UWorld* World = GetWorld())
	{
		if (UGameEventBus* EventBus = World->GetSubsystem<UGameEventBus>())
		{
			EventBus->UnsubscribeAllForObject(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UEditorOverlayComponent::SetVisible(bool bNewVisible)
{
	if (bIsVisible == bNewVisible) return;

	bIsVisible = bNewVisible;

	if (bIsVisible)
	{
		// Becoming visible: rebuild if data changed while hidden
		if (bDataDirty)
		{
			Rebuild();
			bDataDirty = false;
		}
	}
	else
	{
		Clear();
		bDataDirty = true; // so Rebuild() runs when re-shown
	}
}

void UEditorOverlayComponent::RequestDebugLineFlush()
{
	if (AEditorOverlayManager* Manager = GetOverlayManager())
	{
		Manager->FlushAndRebuildDebugLines();
	}
}

AEditorOverlayManager* UEditorOverlayComponent::GetOverlayManager() const
{
	return Cast<AEditorOverlayManager>(GetOwner());
}
