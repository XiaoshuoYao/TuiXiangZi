#include "UI/Editor/LoadLevelDialog.h"
#include "LevelData/LevelSerializer.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

namespace LoadLevelColors
{
	static const FLinearColor SelectedBorder(0.267f, 0.6f, 1.0f, 1.0f);   // #4499FF
	static const FLinearColor SelectedBackground(0.1f, 0.2f, 0.333f, 0.3f);
	static const FLinearColor NormalBackground(0.2f, 0.2f, 0.2f, 1.0f);   // #333333
	static const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);
}

// ============================================================
// NativeConstruct
// ============================================================
void ULoadLevelDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &ULoadLevelDialog::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &ULoadLevelDialog::HandleCancelClicked);
	}
}

// ============================================================
// Setup
// ============================================================
void ULoadLevelDialog::Setup()
{
	if (TitleText)
	{
		TitleText->SetText(NSLOCTEXT("Editor", "LoadLevelTitle", "加载关卡"));
	}
	if (ConfirmButtonText)
	{
		ConfirmButtonText->SetText(NSLOCTEXT("Editor", "Load", "加载"));
	}

	PopulateLevelList();

	// Disable confirm button until a level is selected
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(SelectedIndex >= 0);
	}
}

// ============================================================
// PopulateLevelList
// ============================================================
void ULoadLevelDialog::PopulateLevelList()
{
	if (!LevelListScrollBox) return;

	LevelListScrollBox->ClearChildren();
	EntryButtons.Empty();
	EntryBorders.Empty();
	LevelFileNames.Empty();
	SelectedIndex = -1;

	TArray<FString> FileNames;
	ULevelSerializer::GetAvailableLevelFiles(FileNames);

	// Strip .json extension for display
	for (const FString& FullName : FileNames)
	{
		FString DisplayName = FPaths::GetBaseFilename(FullName);
		LevelFileNames.Add(DisplayName);
	}

	if (LevelFileNames.Num() == 0)
	{
		if (EmptyHintText)
		{
			EmptyHintText->SetVisibility(ESlateVisibility::Visible);
			EmptyHintText->SetText(NSLOCTEXT("Editor", "NoLevels", "没有已保存的关卡"));
		}
		return;
	}

	if (EmptyHintText)
	{
		EmptyHintText->SetVisibility(ESlateVisibility::Collapsed);
	}

	for (int32 i = 0; i < LevelFileNames.Num(); ++i)
	{
		// Border for selection highlight
		UBorder* Border = NewObject<UBorder>(this);
		Border->SetBrushColor(LoadLevelColors::Transparent);
		FSlateBrush BackBrush;
		BackBrush.TintColor = FSlateColor(LoadLevelColors::NormalBackground);
		Border->SetBrush(BackBrush);
		Border->SetPadding(FMargin(8.0f, 6.0f));

		// Button (transparent style)
		UButton* EntryButton = NewObject<UButton>(this);
		FButtonStyle BtnStyle;
		BtnStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
		BtnStyle.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
		BtnStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		EntryButton->SetStyle(BtnStyle);

		// Level name text
		UTextBlock* NameText = NewObject<UTextBlock>(this);
		NameText->SetText(FText::FromString(LevelFileNames[i]));
		FSlateFontInfo Font = NameText->GetFont();
		Font.Size = 14;
		NameText->SetFont(Font);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		EntryButton->AddChild(NameText);
		Border->AddChild(EntryButton);
		LevelListScrollBox->AddChild(Border);

		EntryButtons.Add(EntryButton);
		EntryBorders.Add(Border);
	}

	// Bind click handlers using IsHovered pattern (same as variant buttons)
	for (int32 i = 0; i < EntryButtons.Num(); ++i)
	{
		if (EntryButtons[i])
		{
			EntryButtons[i]->OnClicked.AddDynamic(this, &ULoadLevelDialog::HandleEntryClicked);
		}
	}
}

// ============================================================
// SelectEntry
// ============================================================
void ULoadLevelDialog::SelectEntry(int32 Index)
{
	SelectedIndex = Index;

	for (int32 i = 0; i < EntryBorders.Num(); ++i)
	{
		UBorder* Border = EntryBorders[i];
		if (!Border) continue;

		if (i == Index)
		{
			Border->SetBrushColor(LoadLevelColors::SelectedBorder);
			FSlateBrush SelectedBg;
			SelectedBg.TintColor = FSlateColor(LoadLevelColors::SelectedBackground);
			Border->SetBrush(SelectedBg);
		}
		else
		{
			Border->SetBrushColor(LoadLevelColors::Transparent);
			FSlateBrush NormalBg;
			NormalBg.TintColor = FSlateColor(LoadLevelColors::NormalBackground);
			Border->SetBrush(NormalBg);
		}
	}

	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(SelectedIndex >= 0);
	}
}

// ============================================================
// HandleEntryClicked
// ============================================================
void ULoadLevelDialog::HandleEntryClicked()
{
	for (int32 i = 0; i < EntryButtons.Num(); ++i)
	{
		if (EntryButtons[i] && EntryButtons[i]->IsHovered())
		{
			SelectEntry(i);
			return;
		}
	}
}

// ============================================================
// Confirm / Cancel
// ============================================================
void ULoadLevelDialog::HandleConfirmClicked()
{
	if (SelectedIndex >= 0 && LevelFileNames.IsValidIndex(SelectedIndex))
	{
		OnConfirmed.Broadcast(LevelFileNames[SelectedIndex]);
	}
}

void ULoadLevelDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}
