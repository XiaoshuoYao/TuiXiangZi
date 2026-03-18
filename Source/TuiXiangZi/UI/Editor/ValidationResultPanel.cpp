#include "UI/Editor/ValidationResultPanel.h"
#include "Editor/LevelEditorGameMode.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void UValidationResultPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UValidationResultPanel::HandleCloseClicked);
	}
	if (ForceButton)
	{
		ForceButton->OnClicked.AddDynamic(this, &UValidationResultPanel::HandleForceClicked);
	}
}

void UValidationResultPanel::Setup(const FLevelValidationResult& Result, EValidationContext Context)
{
	if (ResultList)
	{
		ResultList->ClearChildren();
	}

	// Add error entries
	for (const FString& ErrorMsg : Result.Errors)
	{
		AddResultEntry(ErrorMsg, true);
	}

	// Add warning entries
	for (const FString& WarningMsg : Result.Warnings)
	{
		AddResultEntry(WarningMsg, false);
	}

	// Configure force button visibility
	if (ForceButton)
	{
		if (Result.HasErrors())
		{
			ForceButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			ForceButton->SetVisibility(ESlateVisibility::Visible);
			if (ForceButtonText)
			{
				const FText ButtonLabel = (Context == EValidationContext::Save)
					? NSLOCTEXT("Editor", "ForceSave", "\u4ECD\u7136\u4FDD\u5B58")
					: NSLOCTEXT("Editor", "ForceTest", "\u4ECD\u7136\u6D4B\u8BD5");
				ForceButtonText->SetText(ButtonLabel);
			}
		}
	}

	if (TitleText)
	{
		TitleText->SetText(NSLOCTEXT("Editor", "ValidationTitle", "\u9A8C\u8BC1\u7ED3\u679C"));
	}
}

void UValidationResultPanel::HandleCloseClicked()
{
	OnClosed.Broadcast();
}

void UValidationResultPanel::HandleForceClicked()
{
	OnForceConfirmed.Broadcast();
}

void UValidationResultPanel::AddResultEntry(const FString& Message, bool bIsError)
{
	if (!ResultList)
	{
		return;
	}

	// Create horizontal box container
	UHorizontalBox* EntryBox = NewObject<UHorizontalBox>(this);

	// Icon text block
	UTextBlock* IconText = NewObject<UTextBlock>(this);
	if (bIsError)
	{
		IconText->SetText(FText::FromString(TEXT("\u2717 ")));
		IconText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.267f, 0.267f))); // #FF4444
	}
	else
	{
		IconText->SetText(FText::FromString(TEXT("\u26A0 ")));
		IconText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.667f, 0.0f))); // #FFAA00
	}

	UHorizontalBoxSlot* IconSlot = EntryBox->AddChildToHorizontalBox(IconText);
	IconSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Left);
	IconSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	IconSlot->SetPadding(FMargin(4.0f, 2.0f, 0.0f, 2.0f));

	// Message text block
	UTextBlock* MessageTextBlock = NewObject<UTextBlock>(this);
	MessageTextBlock->SetText(FText::FromString(Message));
	if (bIsError)
	{
		MessageTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.267f, 0.267f)));
	}
	else
	{
		MessageTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.667f, 0.0f)));
	}

	UHorizontalBoxSlot* MessageSlot = EntryBox->AddChildToHorizontalBox(MessageTextBlock);
	MessageSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
	MessageSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	MessageSlot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f));

	// Add to scroll box
	ResultList->AddChild(EntryBox);
}
