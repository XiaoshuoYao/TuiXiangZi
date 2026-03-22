#include "Editor/EditorOverlayManager.h"
#include "Editor/EditorOverlayComponent.h"
#include "Editor/Overlays/GridLineOverlay.h"
#include "Editor/Overlays/CoordinateLabelOverlay.h"
#include "Editor/Overlays/TeleporterArrowOverlay.h"
#include "DrawDebugHelpers.h"

AEditorOverlayManager::AEditorOverlayManager()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	GridLineOverlay = CreateDefaultSubobject<UGridLineOverlay>(TEXT("GridLineOverlay"));
	CoordinateLabelOverlay = CreateDefaultSubobject<UCoordinateLabelOverlay>(TEXT("CoordinateLabelOverlay"));
	TeleporterArrowOverlay = CreateDefaultSubobject<UTeleporterArrowOverlay>(TEXT("TeleporterArrowOverlay"));
}

void AEditorOverlayManager::BeginPlay()
{
	Super::BeginPlay();

	// Teleporter arrows are always visible
	TeleporterArrowOverlay->SetVisible(true);

	// Apply initial overlay mode (None by default — grid lines and coords hidden)
	ApplyOverlayMode();
}

void AEditorOverlayManager::CycleOverlayMode()
{
	switch (OverlayMode)
	{
	case EGridOverlayMode::None:         OverlayMode = EGridOverlayMode::GridLines;   break;
	case EGridOverlayMode::GridLines:    OverlayMode = EGridOverlayMode::Coordinates; break;
	case EGridOverlayMode::Coordinates:  OverlayMode = EGridOverlayMode::Both;        break;
	case EGridOverlayMode::Both:         OverlayMode = EGridOverlayMode::None;        break;
	}

	ApplyOverlayMode();
}

void AEditorOverlayManager::ApplyOverlayMode()
{
	bool bWantLines = (OverlayMode == EGridOverlayMode::GridLines || OverlayMode == EGridOverlayMode::Both);
	bool bWantCoords = (OverlayMode == EGridOverlayMode::Coordinates || OverlayMode == EGridOverlayMode::Both);

	GridLineOverlay->SetVisible(bWantLines);
	CoordinateLabelOverlay->SetVisible(bWantCoords);
}

void AEditorOverlayManager::FlushAndRebuildDebugLines()
{
	if (UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}

	// Redraw all visible overlays that use debug lines
	for (UEditorOverlayComponent* Overlay : GetAllOverlays())
	{
		if (Overlay && Overlay->IsVisible() && Overlay->UsesDebugLines())
		{
			Overlay->RedrawDebugLines();
		}
	}
}

TArray<UEditorOverlayComponent*> AEditorOverlayManager::GetAllOverlays() const
{
	TArray<UEditorOverlayComponent*> Result;
	GetComponents<UEditorOverlayComponent>(Result);
	return Result;
}
