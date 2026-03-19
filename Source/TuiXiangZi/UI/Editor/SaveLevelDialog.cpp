#include "UI/Editor/SaveLevelDialog.h"
#include "LevelData/LevelSerializer.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"

namespace SaveLevelColors
{
	static const FLinearColor SelectedBorder(0.267f, 0.6f, 1.0f, 1.0f);   // #4499FF
	static const FLinearColor SelectedBackground(0.1f, 0.2f, 0.333f, 0.3f);
	static const FLinearColor NormalBackground(0.2f, 0.2f, 0.2f, 1.0f);   // #333333
	static const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);
}

// ============================================================
// NativeConstruct
// ============================================================
void USaveLevelDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &USaveLevelDialog::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &USaveLevelDialog::HandleCancelClicked);
	}
	if (FileNameInput)
	{
		FileNameInput->OnTextChanged.AddDynamic(this, &USaveLevelDialog::HandleFileNameChanged);
	}
}

// ============================================================
// Setup
// ============================================================
void USaveLevelDialog::Setup()
{
	if (TitleText)
	{
		TitleText->SetText(NSLOCTEXT("Editor", "SaveLevelTitle", "保存关卡"));
	}

	PopulateExistingLevels();

	if (FileNameInput)
	{
		FileNameInput->SetText(FText::GetEmpty());
		FileNameInput->SetKeyboardFocus();
	}

	UpdateConfirmButton();
}

// ============================================================
// PopulateExistingLevels
// ============================================================
void USaveLevelDialog::PopulateExistingLevels()
{
	if (!ExistingLevelScrollBox) return;

	ExistingLevelScrollBox->ClearChildren();
	EntryButtons.Empty();
	EntryBorders.Empty();
	ExistingFileNames.Empty();
	SelectedExistingIndex = -1;

	TArray<FString> FileNames;
	ULevelSerializer::GetAvailableLevelFiles(FileNames);

	for (const FString& FullName : FileNames)
	{
		ExistingFileNames.Add(FPaths::GetBaseFilename(FullName));
	}

	if (ExistingFileNames.Num() == 0)
	{
		if (EmptyHintText)
		{
			EmptyHintText->SetVisibility(ESlateVisibility::Visible);
			EmptyHintText->SetText(NSLOCTEXT("Editor", "NoExistingLevels", "没有已保存的关卡"));
		}
		return;
	}

	if (EmptyHintText)
	{
		EmptyHintText->SetVisibility(ESlateVisibility::Collapsed);
	}

	for (int32 i = 0; i < ExistingFileNames.Num(); ++i)
	{
		UBorder* Border = NewObject<UBorder>(this);
		Border->SetBrushColor(SaveLevelColors::Transparent);
		FSlateBrush BackBrush;
		BackBrush.TintColor = FSlateColor(SaveLevelColors::NormalBackground);
		Border->SetBrush(BackBrush);
		Border->SetPadding(FMargin(8.0f, 6.0f));

		UButton* EntryButton = NewObject<UButton>(this);
		FButtonStyle BtnStyle;
		BtnStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
		BtnStyle.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
		BtnStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		EntryButton->SetStyle(BtnStyle);

		UTextBlock* NameText = NewObject<UTextBlock>(this);
		NameText->SetText(FText::FromString(ExistingFileNames[i]));
		FSlateFontInfo Font = NameText->GetFont();
		Font.Size = 14;
		NameText->SetFont(Font);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		EntryButton->AddChild(NameText);
		Border->AddChild(EntryButton);
		ExistingLevelScrollBox->AddChild(Border);

		EntryButtons.Add(EntryButton);
		EntryBorders.Add(Border);

		EntryButton->OnClicked.AddDynamic(this, &USaveLevelDialog::HandleExistingEntryClicked);
	}
}

// ============================================================
// HandleExistingEntryClicked — click existing level to fill name
// ============================================================
void USaveLevelDialog::HandleExistingEntryClicked()
{
	for (int32 i = 0; i < EntryButtons.Num(); ++i)
	{
		if (EntryButtons[i] && EntryButtons[i]->IsHovered())
		{
			SelectedExistingIndex = i;

			// Fill the input box with the selected level name
			if (FileNameInput && ExistingFileNames.IsValidIndex(i))
			{
				FileNameInput->SetText(FText::FromString(ExistingFileNames[i]));
			}

			// Update highlight
			for (int32 j = 0; j < EntryBorders.Num(); ++j)
			{
				UBorder* Border = EntryBorders[j];
				if (!Border) continue;

				if (j == i)
				{
					Border->SetBrushColor(SaveLevelColors::SelectedBorder);
					FSlateBrush SelectedBg;
					SelectedBg.TintColor = FSlateColor(SaveLevelColors::SelectedBackground);
					Border->SetBrush(SelectedBg);
				}
				else
				{
					Border->SetBrushColor(SaveLevelColors::Transparent);
					FSlateBrush NormalBg;
					NormalBg.TintColor = FSlateColor(SaveLevelColors::NormalBackground);
					Border->SetBrush(NormalBg);
				}
			}

			UpdateConfirmButton();
			return;
		}
	}
}

// ============================================================
// HandleFileNameChanged — manual typing clears list selection
// ============================================================
void USaveLevelDialog::HandleFileNameChanged(const FText& Text)
{
	// Clear list selection when user types manually
	SelectedExistingIndex = -1;
	for (UBorder* Border : EntryBorders)
	{
		if (Border)
		{
			Border->SetBrushColor(SaveLevelColors::Transparent);
			FSlateBrush NormalBg;
			NormalBg.TintColor = FSlateColor(SaveLevelColors::NormalBackground);
			Border->SetBrush(NormalBg);
		}
	}

	UpdateConfirmButton();
}

// ============================================================
// UpdateConfirmButton
// ============================================================
void USaveLevelDialog::UpdateConfirmButton()
{
	if (!FileNameInput || !ConfirmButton) return;

	FString CurrentName = FileNameInput->GetText().ToString().TrimStartAndEnd();
	bool bHasName = !CurrentName.IsEmpty();

	ConfirmButton->SetIsEnabled(bHasName);

	// Check if name matches an existing file
	if (ConfirmButtonText)
	{
		bool bIsOverwrite = ExistingFileNames.Contains(CurrentName);
		if (bIsOverwrite)
		{
			ConfirmButtonText->SetText(NSLOCTEXT("Editor", "OverwriteSave", "覆盖保存"));
		}
		else
		{
			ConfirmButtonText->SetText(NSLOCTEXT("Editor", "Save", "保存"));
		}
	}
}

// ============================================================
// Confirm / Cancel
// ============================================================
void USaveLevelDialog::HandleConfirmClicked()
{
	if (!FileNameInput) return;

	FString FileName = FileNameInput->GetText().ToString().TrimStartAndEnd();
	if (!FileName.IsEmpty())
	{
		OnConfirmed.Broadcast(FileName);
	}
}

void USaveLevelDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}
