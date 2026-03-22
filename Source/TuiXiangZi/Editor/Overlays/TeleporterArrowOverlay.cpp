#include "Editor/Overlays/TeleporterArrowOverlay.h"
#include "Events/GameEventBus.h"
#include "Events/GameEventTags.h"
#include "Grid/GridManager.h"
#include "Grid/GridTypes.h"
#include "Editor/LevelEditorGameMode.h"
#include "LevelData/LevelDataTypes.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

void UTeleporterArrowOverlay::SubscribeToEvents(UGameEventBus* EventBus)
{
	EventBus->Subscribe(GameEventTags::EditorCellPainted,
		FOnGameEvent::FDelegate::CreateUObject(this, &UTeleporterArrowOverlay::OnCellChanged));
	EventBus->Subscribe(GameEventTags::EditorCellErased,
		FOnGameEvent::FDelegate::CreateUObject(this, &UTeleporterArrowOverlay::OnCellChanged));
	EventBus->Subscribe(GameEventTags::EditorNewLevel,
		FOnGameEvent::FDelegate::CreateUObject(this, &UTeleporterArrowOverlay::OnCellChanged));
	EventBus->Subscribe(GameEventTags::EditorLevelLoaded,
		FOnGameEvent::FDelegate::CreateUObject(this, &UTeleporterArrowOverlay::OnCellChanged));
	EventBus->Subscribe(GameEventTags::EditorTeleporterDirectionChanged,
		FOnGameEvent::FDelegate::CreateUObject(this, &UTeleporterArrowOverlay::OnCellChanged));
	EventBus->Subscribe(GameEventTags::EditorGridBoundsChanged,
		FOnGameEvent::FDelegate::CreateUObject(this, &UTeleporterArrowOverlay::OnGridBoundsChanged));
}

void UTeleporterArrowOverlay::OnGridBoundsChanged(FName EventTag, const FGameEventPayload& Payload)
{
	CachedCellSize = Payload.FloatParam;
}

void UTeleporterArrowOverlay::OnCellChanged(FName EventTag, const FGameEventPayload& Payload)
{
	CollectTeleporterData();

	if (bIsVisible)
	{
		Rebuild();
	}
	else
	{
		bDataDirty = true;
	}
}

void UTeleporterArrowOverlay::CollectTeleporterData()
{
	CachedArrows.Empty();

	AGridManager* GridManager = Cast<AGridManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
	if (!GridManager) return;

	ALevelEditorGameMode* GameMode = Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode());

	TSet<int32> ProcessedGroups;
	FIntRect Bounds = GridManager->GetGridBounds();

	for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; ++Y)
	{
		for (int32 X = Bounds.Min.X; X < Bounds.Max.X; ++X)
		{
			FIntPoint Pos(X, Y);
			if (!GridManager->HasCell(Pos)) continue;
			FGridCell Cell = GridManager->GetCell(Pos);
			if (Cell.CellType != EGridCellType::Teleporter || Cell.GroupId < 0) continue;
			if (ProcessedGroups.Contains(Cell.GroupId)) continue;
			ProcessedGroups.Add(Cell.GroupId);

			// Find both teleporters in this group
			TArray<FIntPoint> Positions;
			TArray<int32> ExtraParams;
			for (int32 Y2 = Bounds.Min.Y; Y2 < Bounds.Max.Y; ++Y2)
			{
				for (int32 X2 = Bounds.Min.X; X2 < Bounds.Max.X; ++X2)
				{
					FIntPoint P2(X2, Y2);
					if (!GridManager->HasCell(P2)) continue;
					FGridCell C2 = GridManager->GetCell(P2);
					if (C2.CellType == EGridCellType::Teleporter && C2.GroupId == Cell.GroupId)
					{
						Positions.Add(P2);
						ExtraParams.Add(C2.ExtraParam);
					}
				}
			}

			if (Positions.Num() != 2) continue;

			FLinearColor ArrowColor(0.0f, 0.8f, 0.8f); // default cyan
			if (GameMode)
			{
				FMechanismGroupStyleData Style = GameMode->GetGroupStyle(Cell.GroupId);
				if (Style.GroupId >= 0)
				{
					ArrowColor = Style.BaseColor;
				}
			}

			FTeleporterArrowData Arrow;
			Arrow.PosA = Positions[0];
			Arrow.PosB = Positions[1];
			Arrow.ExtraParamA = ExtraParams[0];
			Arrow.Color = ArrowColor;
			CachedArrows.Add(Arrow);
		}
	}
}

void UTeleporterArrowOverlay::Rebuild()
{
	RequestDebugLineFlush();
	// FlushAndRebuildDebugLines will call RedrawDebugLines on all visible overlays
}

void UTeleporterArrowOverlay::Clear()
{
	CachedArrows.Empty();
	RequestDebugLineFlush();
}

void UTeleporterArrowOverlay::RedrawDebugLines()
{
	if (!bIsVisible || CachedArrows.Num() == 0) return;
	DrawArrows();
}

void UTeleporterArrowOverlay::DrawArrows()
{
	UWorld* World = GetWorld();
	if (!World) return;

	float HalfCell = CachedCellSize * 0.5f;

	for (const FTeleporterArrowData& Arrow : CachedArrows)
	{
		FVector CenterA(Arrow.PosA.X * CachedCellSize + HalfCell, Arrow.PosA.Y * CachedCellSize + HalfCell, ArrowZ);
		FVector CenterB(Arrow.PosB.X * CachedCellSize + HalfCell, Arrow.PosB.Y * CachedCellSize + HalfCell, ArrowZ);

		FVector Dir = (CenterB - CenterA).GetSafeNormal();
		FVector Perp = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		FColor ArrowColor = Arrow.Color.ToFColor(true);

		bool bArrowAtB = (Arrow.ExtraParamA == 0 || Arrow.ExtraParamA == 1);
		bool bArrowAtA = (Arrow.ExtraParamA == 0 || Arrow.ExtraParamA == 2);

		// Main line
		DrawDebugLine(World, CenterA, CenterB, ArrowColor, true, -1.0f, SDPG_World, ArrowThickness);

		// Arrowhead at B
		if (bArrowAtB)
		{
			FVector Tip = CenterB;
			FVector Base = Tip - Dir * ArrowHeadSize;
			DrawDebugLine(World, Tip, Base - Perp * ArrowHeadSize * 0.5f, ArrowColor, true, -1.0f, SDPG_World, ArrowThickness);
			DrawDebugLine(World, Tip, Base + Perp * ArrowHeadSize * 0.5f, ArrowColor, true, -1.0f, SDPG_World, ArrowThickness);
		}

		// Arrowhead at A
		if (bArrowAtA)
		{
			FVector Tip = CenterA;
			FVector Base = Tip + Dir * ArrowHeadSize;
			DrawDebugLine(World, Tip, Base - Perp * ArrowHeadSize * 0.5f, ArrowColor, true, -1.0f, SDPG_World, ArrowThickness);
			DrawDebugLine(World, Tip, Base + Perp * ArrowHeadSize * 0.5f, ArrowColor, true, -1.0f, SDPG_World, ArrowThickness);
		}
	}
}
