#include "GroupEntryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UGroupEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RowButton)
	{
		RowButton->OnClicked.AddDynamic(this, &UGroupEntryWidget::HandleRowClicked);
		RowButton->OnHovered.AddDynamic(this, &UGroupEntryWidget::HandleRowHovered);
		RowButton->OnUnhovered.AddDynamic(this, &UGroupEntryWidget::HandleRowUnhovered);
	}
	if (ColorEditButton)
	{
		ColorEditButton->OnClicked.AddDynamic(this, &UGroupEntryWidget::HandleColorEditClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked.AddDynamic(this, &UGroupEntryWidget::HandleDeleteClicked);
	}
}

void UGroupEntryWidget::Setup(int32 InGroupId, const FText& InDisplayName, FLinearColor InBaseColor)
{
	GroupId = InGroupId;

	if (NameText)
	{
		NameText->SetText(InDisplayName);
	}
	if (ColorPreview)
	{
		ColorPreview->SetColorAndOpacity(InBaseColor);
	}
}

void UGroupEntryWidget::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;

	if (SelectionMark)
	{
		SelectionMark->SetText(bSelected ? FText::FromString(TEXT("\u25B6")) : FText::FromString(TEXT(" ")));
	}
	if (RowButton)
	{
		// Selected: blue tint #2A4466, Unselected: transparent
		FLinearColor BgColor = bSelected
			? FLinearColor(0.164f, 0.267f, 0.4f, 1.0f)
			: FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

		FButtonStyle Style = RowButton->GetStyle();
		FSlateBrush NormalBrush;
		NormalBrush.TintColor = FSlateColor(BgColor);
		Style.Normal = NormalBrush;
		Style.Pressed = NormalBrush;
		RowButton->SetStyle(Style);
	}
}

void UGroupEntryWidget::SetBaseColor(FLinearColor NewColor)
{
	if (ColorPreview)
	{
		ColorPreview->SetColorAndOpacity(NewColor);
	}
}

void UGroupEntryWidget::SetInteractionEnabled(bool bEnabled)
{
	if (RowButton)
	{
		RowButton->SetIsEnabled(bEnabled);
	}
	if (ColorEditButton)
	{
		ColorEditButton->SetIsEnabled(bEnabled);
	}
	if (DeleteButton)
	{
		DeleteButton->SetIsEnabled(bEnabled);
	}
}

void UGroupEntryWidget::HandleRowClicked()
{
	OnRowClicked.Broadcast(GroupId);
}

void UGroupEntryWidget::HandleColorEditClicked()
{
	OnColorEditClicked.Broadcast(GroupId);
}

void UGroupEntryWidget::HandleDeleteClicked()
{
	OnDeleteClicked.Broadcast(GroupId);
}

void UGroupEntryWidget::HandleRowHovered()
{
	if (!bIsSelected && RowButton)
	{
		FButtonStyle Style = RowButton->GetStyle();
		FSlateBrush HoverBrush;
		HoverBrush.TintColor = FSlateColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f)); // #333333
		Style.Hovered = HoverBrush;
		RowButton->SetStyle(Style);
	}
}

void UGroupEntryWidget::HandleRowUnhovered()
{
	if (!bIsSelected && RowButton)
	{
		FButtonStyle Style = RowButton->GetStyle();
		FSlateBrush NormalBrush;
		NormalBrush.TintColor = FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)); // transparent
		Style.Hovered = NormalBrush;
		RowButton->SetStyle(Style);
	}
}
