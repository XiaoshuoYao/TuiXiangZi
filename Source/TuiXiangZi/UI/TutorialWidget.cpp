#include "UI/TutorialWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UTutorialWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (NextButton)
	{
		NextButton->OnClicked.AddDynamic(this, &UTutorialWidget::OnNextButtonClicked);
	}
}

void UTutorialWidget::SetTutorialText(const FText& Text)
{
	if (TutorialText)
	{
		TutorialText->SetText(Text);
	}
}

void UTutorialWidget::SetButtonText(const FText& Text)
{
	if (NextButtonText)
	{
		NextButtonText->SetText(Text);
	}
}

void UTutorialWidget::SetNextButtonVisible(bool bVisible)
{
	if (NextButton)
	{
		NextButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTutorialWidget::OnNextButtonClicked()
{
	OnAdvanced.Broadcast();
}
