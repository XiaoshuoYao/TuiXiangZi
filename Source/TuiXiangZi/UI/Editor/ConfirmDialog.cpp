#include "UI/Editor/ConfirmDialog.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UConfirmDialog::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UConfirmDialog::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UConfirmDialog::HandleCancelClicked);
	}
}

void UConfirmDialog::Setup(const FText& Title, const FText& Message,
                           const FText& ConfirmText, const FText& CancelText)
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (MessageText)
	{
		MessageText->SetText(Message);
	}
	if (ConfirmButtonText)
	{
		ConfirmButtonText->SetText(ConfirmText);
	}
	if (CancelButtonText)
	{
		CancelButtonText->SetText(CancelText);
	}
}

void UConfirmDialog::HandleConfirmClicked()
{
	OnConfirmed.Broadcast();
}

void UConfirmDialog::HandleCancelClicked()
{
	OnCancelled.Broadcast();
}
