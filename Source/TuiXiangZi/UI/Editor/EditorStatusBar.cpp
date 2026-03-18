#include "UI/Editor/EditorStatusBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"

void UEditorStatusBar::NativeConstruct()
{
	Super::NativeConstruct();


	RefreshModeText(EEditorMode::Normal);
	RefreshBrushText(EEditorBrush::Floor);
	RefreshStats(0, 0, 0);

	if (TempMessageText)
	{
		TempMessageText->SetText(FText::GetEmpty());
	}
}

void UEditorStatusBar::RefreshModeText(EEditorMode Mode)
{
	if (!ModeText)
	{
		return;
	}

	switch (Mode)
	{
	case EEditorMode::Normal:
		ModeText->SetText(FText::FromString(TEXT("普通模式")));
		ModeText->SetColorAndOpacity(FLinearColor::White);
		StopBlinkIfNeeded();
		break;

	case EEditorMode::PlacingPlatesForDoor:
		ModeText->SetText(FText::FromString(TEXT("放置压力板 — 右键结束")));
		ModeText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.0f));
		if (BlinkAnimation && !bIsBlinking)
		{
			PlayAnimation(BlinkAnimation, 0.0f, 0);
			bIsBlinking = true;
		}
		break;

	case EEditorMode::EditingDoorGroup:
		ModeText->SetText(FText::FromString(TEXT("编辑分组")));
		ModeText->SetColorAndOpacity(FLinearColor(0.3f, 0.6f, 1.0f));
		StopBlinkIfNeeded();
		break;
	}
}

void UEditorStatusBar::RefreshBrushText(EEditorBrush Brush)
{
	if (!BrushText)
	{
		return;
	}

	// Brush display name mapping
	static const TMap<EEditorBrush, FText> BrushNames = {
		{EEditorBrush::Floor,         FText::FromString(TEXT("地板"))},
		{EEditorBrush::Wall,          FText::FromString(TEXT("墙壁"))},
		{EEditorBrush::Ice,           FText::FromString(TEXT("冰面"))},
		{EEditorBrush::Goal,          FText::FromString(TEXT("目标"))},
		{EEditorBrush::Door,          FText::FromString(TEXT("机关门"))},
		{EEditorBrush::PressurePlate, FText::FromString(TEXT("压力板"))},
		{EEditorBrush::BoxSpawn,      FText::FromString(TEXT("箱子生成"))},
		{EEditorBrush::PlayerStart,   FText::FromString(TEXT("玩家起点"))},
		{EEditorBrush::Eraser,        FText::FromString(TEXT("橡皮擦"))}
	};

	// Brush color mapping
	static const TMap<EEditorBrush, FLinearColor> BrushColors = {
		{EEditorBrush::Floor,         FLinearColor::White},
		{EEditorBrush::Wall,          FLinearColor(0.533f, 0.533f, 0.533f)},    // #888888
		{EEditorBrush::Ice,           FLinearColor(0.4f, 0.8f, 1.0f)},          // #66CCFF
		{EEditorBrush::Goal,          FLinearColor(1.0f, 0.267f, 0.267f)},      // #FF4444
		{EEditorBrush::Door,          FLinearColor(0.667f, 0.4f, 1.0f)},        // #AA66FF
		{EEditorBrush::PressurePlate, FLinearColor(1.0f, 0.533f, 0.0f)},        // #FF8800
		{EEditorBrush::BoxSpawn,      FLinearColor(1.0f, 0.667f, 0.0f)},        // #FFAA00
		{EEditorBrush::PlayerStart,   FLinearColor(0.267f, 1.0f, 0.267f)},      // #44FF44
		{EEditorBrush::Eraser,        FLinearColor(1.0f, 0.267f, 0.267f)}       // #FF4444
	};

	const FText* FoundName = BrushNames.Find(Brush);
	const FLinearColor* FoundColor = BrushColors.Find(Brush);

	FText DisplayName = FoundName ? *FoundName : FText::FromString(TEXT("未知"));
	FLinearColor DisplayColor = FoundColor ? *FoundColor : FLinearColor::White;

	BrushText->SetText(FText::Format(
		NSLOCTEXT("EditorStatusBar", "BrushFormat", "笔刷: {0}"),
		DisplayName));
	BrushText->SetColorAndOpacity(FSlateColor(DisplayColor));
}

void UEditorStatusBar::RefreshStats(int32 CellCount, int32 BoxCount, int32 GroupCount)
{
	if (!StatsText)
	{
		return;
	}

	StatsText->SetText(FText::Format(
		NSLOCTEXT("EditorStatusBar", "StatsFormat", "格子: {0}  箱子: {1}  分组: {2}"),
		FText::AsNumber(CellCount),
		FText::AsNumber(BoxCount),
		FText::AsNumber(GroupCount)));
	StatsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
}

void UEditorStatusBar::ShowTemporaryMessage(const FText& Message, float Duration)
{
	if (!TempMessageText)
	{
		return;
	}

	TempMessageText->SetText(Message);
	TempMessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.0f, 0.5f)));

	// Clear previous timer if active
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TempMessageTimerHandle);
		World->GetTimerManager().SetTimer(
			TempMessageTimerHandle,
			this,
			&UEditorStatusBar::ClearTemporaryMessage,
			Duration,
			false);
	}
}

void UEditorStatusBar::ClearTemporaryMessage()
{
	if (TempMessageText)
	{
		TempMessageText->SetText(FText::GetEmpty());
	}
}

void UEditorStatusBar::StopBlinkIfNeeded()
{
	if (bIsBlinking && BlinkAnimation)
	{
		StopAnimation(BlinkAnimation);
		bIsBlinking = false;
	}
}
