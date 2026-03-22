#include "Editor/Overlays/CoordinateLabelOverlay.h"
#include "Events/GameEventBus.h"
#include "Events/GameEventTags.h"
#include "Engine/TextRenderActor.h"
#include "Components/TextRenderComponent.h"

void UCoordinateLabelOverlay::SubscribeToEvents(UGameEventBus* EventBus)
{
	EventBus->Subscribe(GameEventTags::EditorGridBoundsChanged,
		FOnGameEvent::FDelegate::CreateUObject(this, &UCoordinateLabelOverlay::OnGridBoundsChanged));
}

void UCoordinateLabelOverlay::OnGridBoundsChanged(FName EventTag, const FGameEventPayload& Payload)
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

void UCoordinateLabelOverlay::Rebuild()
{
	Clear();
	if (!bHasData || !bIsVisible) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FIntPoint PaddedMin = CachedMin - FIntPoint(PaddingCells, PaddingCells);
	FIntPoint PaddedMax = CachedMax + FIntPoint(PaddingCells, PaddingCells);

	float LabelZ = 60.0f;
	float HalfCell = CachedCellSize * 0.5f;

	for (int32 X = PaddedMin.X; X < PaddedMax.X; ++X)
	{
		for (int32 Y = PaddedMin.Y; Y < PaddedMax.Y; ++Y)
		{
			FVector CellCenter(X * CachedCellSize + HalfCell, Y * CachedCellSize + HalfCell, LabelZ);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			ATextRenderActor* TextActor = World->SpawnActor<ATextRenderActor>(
				CellCenter, FRotator(90.0f, 0.0f, 180.0f), SpawnParams);
			if (!TextActor) continue;

			UTextRenderComponent* TextComp = TextActor->GetTextRender();
			if (TextComp)
			{
				TextComp->SetText(FText::FromString(FString::Printf(TEXT("%d,%d"), X, Y)));
				TextComp->SetWorldSize(CachedCellSize * 0.18f);
				TextComp->SetTextRenderColor(FColor(200, 200, 200, 200));
				TextComp->SetHorizontalAlignment(EHTA_Center);
				TextComp->SetVerticalAlignment(EVRTA_TextCenter);
			}

			LabelActors.Add(TextActor);
		}
	}
}

void UCoordinateLabelOverlay::Clear()
{
	for (ATextRenderActor* Actor : LabelActors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	LabelActors.Empty();
}
