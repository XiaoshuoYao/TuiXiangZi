#include "Editor/Overlays/GridLineOverlay.h"
#include "Events/GameEventBus.h"
#include "Events/GameEventTags.h"
#include "DrawDebugHelpers.h"

void UGridLineOverlay::SubscribeToEvents(UGameEventBus* EventBus)
{
	EventBus->Subscribe(GameEventTags::EditorGridBoundsChanged,
		FOnGameEvent::FDelegate::CreateUObject(this, &UGridLineOverlay::OnGridBoundsChanged));
}

void UGridLineOverlay::OnGridBoundsChanged(FName EventTag, const FGameEventPayload& Payload)
{
	CachedMin = Payload.FromPos;
	CachedMax = Payload.ToPos;
	CachedCellSize = Payload.FloatParam;
	bHasData = true;

	if (bIsVisible)
	{
		Rebuild();
	}
	else
	{
		bDataDirty = true;
	}
}

void UGridLineOverlay::Rebuild()
{
	RequestDebugLineFlush();
	// FlushAndRebuildDebugLines will call RedrawDebugLines on all visible overlays
}

void UGridLineOverlay::Clear()
{
	if (bHasData)
	{
		RequestDebugLineFlush();
	}
}

void UGridLineOverlay::RedrawDebugLines()
{
	if (!bHasData || !bIsVisible) return;
	DrawLines();
}

void UGridLineOverlay::DrawLines()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FIntPoint PaddedMin = CachedMin - FIntPoint(PaddingCells, PaddingCells);
	FIntPoint PaddedMax = CachedMax + FIntPoint(PaddingCells, PaddingCells);
	FColor LineColor(160, 160, 160);

	for (int32 X = PaddedMin.X; X <= PaddedMax.X; ++X)
	{
		FVector Start(X * CachedCellSize, PaddedMin.Y * CachedCellSize, LineZ);
		FVector End(X * CachedCellSize, PaddedMax.Y * CachedCellSize, LineZ);
		DrawDebugLine(World, Start, End, LineColor, true, -1.0f, SDPG_World, LineThickness);
	}

	for (int32 Y = PaddedMin.Y; Y <= PaddedMax.Y; ++Y)
	{
		FVector Start(PaddedMin.X * CachedCellSize, Y * CachedCellSize, LineZ);
		FVector End(PaddedMax.X * CachedCellSize, Y * CachedCellSize, LineZ);
		DrawDebugLine(World, Start, End, LineColor, true, -1.0f, SDPG_World, LineThickness);
	}
}
