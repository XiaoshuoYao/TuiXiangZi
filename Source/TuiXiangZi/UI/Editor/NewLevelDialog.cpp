#include "UI/Editor/NewLevelDialog.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UNewLevelDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (WidthSpinBox)
	{
		WidthSpinBox->SetMinValue(3.0f);
		WidthSpinBox->SetMinSliderValue(3.0f);
		WidthSpinBox->SetMaxValue(30.0f);
		WidthSpinBox->SetMaxSliderValue(30.0f);
		WidthSpinBox->SetValue(8.0f);
		WidthSpinBox->Delta = 1.0f;
		WidthSpinBox->OnValueChanged.AddDynamic(this, &UNewLevelDialog::HandleWidthChanged);
	}

	if (HeightSpinBox)
	{
		HeightSpinBox->SetMinValue(3.0f);
		HeightSpinBox->SetMinSliderValue(3.0f);
		HeightSpinBox->SetMaxValue(30.0f);
		HeightSpinBox->SetMaxSliderValue(30.0f);
		HeightSpinBox->SetValue(8.0f);
		HeightSpinBox->Delta = 1.0f;
		HeightSpinBox->OnValueChanged.AddDynamic(this, &UNewLevelDialog::HandleHeightChanged);
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UNewLevelDialog::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UNewLevelDialog::HandleCancelClicked);
	}
	UpdatePreview();

	if (WidthSpinBox)
	{
		WidthSpinBox->SetKeyboardFocus();
	}
}

void UNewLevelDialog::HandleWidthChanged(float Value)
{
	UpdatePreview();
}

void UNewLevelDialog::HandleHeightChanged(float Value)
{
	UpdatePreview();
}

void UNewLevelDialog::UpdatePreview()
{
	if (!PreviewText || !WidthSpinBox || !HeightSpinBox)
	{
		return;
	}

	const int32 W = FMath::RoundToInt32(WidthSpinBox->GetValue());
	const int32 H = FMath::RoundToInt32(HeightSpinBox->GetValue());
	const int32 Total = W * H;

	const FText PreviewFmt = FText::Format(
		NSLOCTEXT("Editor", "NewLevelPreview", "{0} \u00D7 {1} = {2} \u683C"),
		FText::AsNumber(W), FText::AsNumber(H), FText::AsNumber(Total));

	PreviewText->SetText(PreviewFmt);
}

void UNewLevelDialog::HandleConfirmClicked()
{
	if (!WidthSpinBox || !HeightSpinBox)
	{
		return;
	}

	const int32 W = FMath::RoundToInt32(WidthSpinBox->GetValue());
	const int32 H = FMath::RoundToInt32(HeightSpinBox->GetValue());
	OnConfirmed.Broadcast(W, H);
}

void UNewLevelDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}
