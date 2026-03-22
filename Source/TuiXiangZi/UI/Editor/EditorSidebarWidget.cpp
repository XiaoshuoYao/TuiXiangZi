#include "UI/Editor/EditorSidebarWidget.h"
#include "Grid/TileStyleCatalog.h"

#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"
#include "Engine/Texture2D.h"

// Brush configuration from shared descriptor table
static auto GetBrushConfigs() { return BrushUtils::GetAllBrushDescriptors(); }
static int32 GetBrushCount() { return GetBrushConfigs().Num(); }

// ============================================================
// Color constants
// ============================================================
namespace SidebarColors
{
	static const FLinearColor SelectedBorder(0.267f, 0.6f, 1.0f, 1.0f);   // #4499FF
	static const FLinearColor SelectedBackground(0.1f, 0.2f, 0.333f, 0.3f); // #1A3355 30%
	static const FLinearColor NormalBackground(0.2f, 0.2f, 0.2f, 1.0f);    // #333333
	static const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);
	static const FLinearColor ShortcutGray(0.5f, 0.5f, 0.5f, 1.0f);
	static const FLinearColor VariantSelectedBorder(0.267f, 0.6f, 1.0f, 1.0f);
	static const FLinearColor VariantNormalBorder(0.0f, 0.0f, 0.0f, 0.0f);
}

// ============================================================
// NativeConstruct
// ============================================================
void UEditorSidebarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CreateBrushButtons();

	// Hide variant panel until catalog is provided
	if (VariantPanel)
	{
		VariantPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Set initial brush highlight
	SetActiveBrush(EEditorBrush::Floor);
}

// ============================================================
// CreateBrushButtons
// ============================================================
void UEditorSidebarWidget::CreateBrushButtons()
{
	if (!BrushButtonContainer) return;

	BrushButtons.Empty();
	BrushButtonBorders.Empty();
	BrushOrder.Empty();

	for (int32 i = 0; i < GetBrushCount(); ++i)
	{
		const FBrushDescriptor& Cfg = GetBrushConfigs()[i];

		// --- Border (outer frame for highlight) ---
		UBorder* Border = NewObject<UBorder>(this);
		Border->SetBrushColor(SidebarColors::Transparent);
		Border->SetContentColorAndOpacity(FLinearColor::White);

		FSlateBrush BackBrush;
		BackBrush.TintColor = FSlateColor(SidebarColors::NormalBackground);
		Border->SetBrush(BackBrush);

		Border->SetPadding(FMargin(4.0f, 2.0f));

		// --- Button wrapping the border content ---
		UButton* Button = NewObject<UButton>(this);

		// Make button style transparent so Border controls visuals
		FButtonStyle TransparentStyle;
		TransparentStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		TransparentStyle.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		TransparentStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		Button->SetStyle(TransparentStyle);

		// --- HorizontalBox ---
		UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);

		// Icon (colored square)
		UImage* Icon = NewObject<UImage>(this);
		Icon->SetColorAndOpacity(Cfg.IconColor);
		UHorizontalBoxSlot* IconSlot = HBox->AddChildToHorizontalBox(Icon);
		IconSlot->SetPadding(FMargin(4.0f, 6.0f, 0.0f, 6.0f));
		IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		Icon->SetDesiredSizeOverride(FVector2D(24.0f, 24.0f));

		// Spacer
		USpacer* Spacer = NewObject<USpacer>(this);
		Spacer->SetSize(FVector2D(8.0f, 1.0f));
		UHorizontalBoxSlot* SpacerSlot = HBox->AddChildToHorizontalBox(Spacer);
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

		// Name text
		UTextBlock* NameText = NewObject<UTextBlock>(this);
		NameText->SetText(FText::FromString(Cfg.DisplayName));
		FSlateFontInfo NameFont = NameText->GetFont();
		NameFont.Size = 14;
		NameText->SetFont(NameFont);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UHorizontalBoxSlot* NameSlot = HBox->AddChildToHorizontalBox(NameText);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);

		// Shortcut text
		UTextBlock* ShortcutText = NewObject<UTextBlock>(this);
		ShortcutText->SetText(FText::FromString(Cfg.Shortcut));
		FSlateFontInfo ShortcutFont = ShortcutText->GetFont();
		ShortcutFont.Size = 12;
		ShortcutText->SetFont(ShortcutFont);
		ShortcutText->SetColorAndOpacity(FSlateColor(SidebarColors::ShortcutGray));
		UHorizontalBoxSlot* ShortcutSlot = HBox->AddChildToHorizontalBox(ShortcutText);
		ShortcutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ShortcutSlot->SetVerticalAlignment(VAlign_Center);
		ShortcutSlot->SetHorizontalAlignment(HAlign_Right);
		ShortcutSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));

		// Assemble: Button contains HBox, Border contains Button
		Button->AddChild(HBox);
		Border->AddChild(Button);

		// Add to container
		UVerticalBoxSlot* BrushSlot = BrushButtonContainer->AddChildToVerticalBox(Border);
		BrushSlot->SetPadding(FMargin(0.0f, 1.0f));

		// Store references
		BrushButtons.Add(Button);
		BrushButtonBorders.Add(Border);
		BrushOrder.Add(Cfg.Brush);
	}

	// Bind all brush buttons to the same handler — source identified via IsHovered()
	for (UButton* Btn : BrushButtons)
	{
		if (Btn)
		{
			Btn->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleAnyBrushButtonClicked);
		}
	}
}

// ============================================================
// Brush click handler — identifies source via IsHovered()
// ============================================================
void UEditorSidebarWidget::HandleAnyBrushButtonClicked()
{
	for (int32 i = 0; i < BrushButtons.Num(); ++i)
	{
		if (BrushButtons[i] && BrushButtons[i]->IsHovered())
		{
			if (BrushOrder.IsValidIndex(i))
			{
				HandleBrushButtonClicked(BrushOrder[i]);
			}
			return;
		}
	}
}

void UEditorSidebarWidget::HandleBrushButtonClicked(EEditorBrush Brush)
{
	OnBrushSelected.Broadcast(Brush);
}

// ============================================================
// SetActiveBrush
// ============================================================
void UEditorSidebarWidget::SetActiveBrush(EEditorBrush Brush)
{
	CurrentBrush = Brush;

	// Update button highlights
	for (int32 i = 0; i < BrushButtons.Num(); ++i)
	{
		if (!BrushButtonBorders.IsValidIndex(i)) continue;

		UBorder* Border = BrushButtonBorders[i];
		if (!Border) continue;

		if (BrushOrder.IsValidIndex(i) && BrushOrder[i] == Brush)
		{
			// Selected state
			Border->SetBrushColor(SidebarColors::SelectedBorder);
			FSlateBrush SelectedBg;
			SelectedBg.TintColor = FSlateColor(SidebarColors::SelectedBackground);
			Border->SetBrush(SelectedBg);
		}
		else
		{
			// Normal state
			Border->SetBrushColor(SidebarColors::Transparent);
			FSlateBrush NormalBg;
			NormalBg.TintColor = FSlateColor(SidebarColors::NormalBackground);
			Border->SetBrush(NormalBg);
		}
	}

	RefreshVariantPanel(Brush);
}

// ============================================================
// SetEnabled
// ============================================================
void UEditorSidebarWidget::SetEnabled(bool bEnabled)
{
	for (UButton* Btn : BrushButtons)
	{
		if (Btn)
		{
			Btn->SetIsEnabled(bEnabled);
		}
	}

	SetRenderOpacity(bEnabled ? 1.0f : 0.5f);
}

// ============================================================
// InitializeWithCatalog
// ============================================================
void UEditorSidebarWidget::InitializeWithCatalog(UTileStyleCatalog* Catalog)
{
	TileStyleCatalog = Catalog;

	// Fallback: in packaged builds the widget may construct before GameMode::BeginPlay
	// has initialized GridManagerRef, so the catalog can be null at this point.
	if (!TileStyleCatalog)
	{
		TileStyleCatalog = LoadObject<UTileStyleCatalog>(
			nullptr, TEXT("/Game/Misc/DA_DefaultTileStyles.DA_DefaultTileStyles"));
	}

	RefreshVariantPanel(CurrentBrush);
}

// ============================================================
// EnterPlateMode
// ============================================================
void UEditorSidebarWidget::EnterPlateMode()
{
	// Disable brush buttons, but keep Door and PressurePlate enabled
	// so user can freely add doors or plates to the current group
	for (int32 i = 0; i < BrushButtons.Num(); ++i)
	{
		if (BrushButtons[i])
		{
			bool bKeepEnabled = (i < BrushOrder.Num() &&
				(BrushOrder[i] == EEditorBrush::Door || BrushOrder[i] == EEditorBrush::PressurePlate));
			BrushButtons[i]->SetIsEnabled(bKeepEnabled);
		}
	}

	// Show pressure plate variants by default
	RefreshVariantPanel(EEditorBrush::PressurePlate);
}

// ============================================================
// ExitPlateMode
// ============================================================
void UEditorSidebarWidget::ExitPlateMode()
{
	// Re-enable brush buttons
	for (UButton* Btn : BrushButtons)
	{
		if (Btn)
		{
			Btn->SetIsEnabled(true);
		}
	}

	// Restore variant panel for current brush
	RefreshVariantPanel(CurrentBrush);
}

// ============================================================
// BrushToCellType
// ============================================================
EGridCellType UEditorSidebarWidget::BrushToCellType(EEditorBrush Brush)
{
	return BrushUtils::BrushToCellType(Brush);
}

// ============================================================
// RefreshVariantPanel
// ============================================================
void UEditorSidebarWidget::RefreshVariantPanel(EEditorBrush Brush)
{
	if (!VariantPanel || !VariantGrid) return;

	EGridCellType CellType = BrushToCellType(Brush);

	auto HideVariantPanel = [&]()
	{
		VariantPanel->SetVisibility(ESlateVisibility::Collapsed);
		VariantGrid->ClearChildren();
		VariantStyleIds.Empty();
		VariantBorders.Empty();
		VariantButtons.Empty();
		CurrentVisualStyleId = NAME_None;
		OnVariantSelected.Broadcast(NAME_None);
	};

	if (CellType == EGridCellType::Empty || !TileStyleCatalog)
	{
		HideVariantPanel();
		return;
	}

	TArray<const FTileVisualStyle*> Styles = TileStyleCatalog->GetStylesForType(CellType);

	if (Styles.Num() == 0)
	{
		HideVariantPanel();
		return;
	}

	VariantPanel->SetVisibility(ESlateVisibility::Visible);
	VariantGrid->ClearChildren();
	VariantStyleIds.Empty();
	VariantBorders.Empty();
	VariantButtons.Empty();

	// Determine initial selection
	FName SelectedId;
	if (const FName* Found = LastSelectedVariant.Find(Brush))
	{
		SelectedId = *Found;
	}
	else
	{
		SelectedId = Styles[0]->StyleId;
	}

	for (int32 i = 0; i < Styles.Num(); ++i)
	{
		const FTileVisualStyle* Style = Styles[i];

		// Border (for selection highlight)
		UBorder* ThumbBorder = NewObject<UBorder>(this);
		ThumbBorder->SetPadding(FMargin(2.0f));
		ThumbBorder->SetBrushColor(SidebarColors::VariantNormalBorder);

		// VerticalBox inside border
		UVerticalBox* VBox = NewObject<UVerticalBox>(this);

		// Thumbnail image
		UImage* ThumbImage = NewObject<UImage>(this);
		ThumbImage->SetDesiredSizeOverride(FVector2D(72.0f, 72.0f));

		if (Style->Thumbnail)
		{
			FSlateBrush ThumbBrush;
			ThumbBrush.SetResourceObject(Style->Thumbnail);
			ThumbBrush.ImageSize = FVector2D(72.0f, 72.0f);
			ThumbImage->SetBrush(ThumbBrush);
		}

		UVerticalBoxSlot* ImgSlot = VBox->AddChildToVerticalBox(ThumbImage);
		ImgSlot->SetHorizontalAlignment(HAlign_Center);

		// Display name text
		UTextBlock* NameText = NewObject<UTextBlock>(this);
		NameText->SetText(FText::FromString(Style->DisplayName));
		FSlateFontInfo SmallFont = NameText->GetFont();
		SmallFont.Size = 10;
		NameText->SetFont(SmallFont);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NameText->SetJustification(ETextJustify::Center);

		UVerticalBoxSlot* TextSlot = VBox->AddChildToVerticalBox(NameText);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

		// Wrap in a button for click handling
		UButton* ThumbButton = NewObject<UButton>(this);
		FButtonStyle ThumbBtnStyle;
		ThumbBtnStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		ThumbBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.1f));
		ThumbBtnStyle.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
		ThumbBtnStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		ThumbButton->SetStyle(ThumbBtnStyle);
		ThumbButton->AddChild(VBox);

		// Set tooltip
		ThumbButton->SetToolTipText(FText::FromString(Style->DisplayName));

		ThumbBorder->AddChild(ThumbButton);

		// Add to grid
		UUniformGridSlot* GridSlot = VariantGrid->AddChildToUniformGrid(ThumbBorder, i / 2, i % 2);
		(void)GridSlot;

		// Bind click: all variant buttons share HandleVariantButtonClicked,
		// which identifies the source by checking IsHovered().
		ThumbButton->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleVariantButtonClicked);

		// Store mapping
		VariantStyleIds.Add(Style->StyleId);
		VariantBorders.Add(ThumbBorder);
		VariantButtons.Add(ThumbButton);

		// Highlight if selected
		if (Style->StyleId == SelectedId)
		{
			ThumbBorder->SetBrushColor(SidebarColors::VariantSelectedBorder);
		}
	}

	// Trigger initial selection
	SelectVariant(SelectedId);
}

// ============================================================
// HandleVariantButtonClicked
// ============================================================
void UEditorSidebarWidget::HandleVariantButtonClicked()
{
	// Identify which variant button was clicked by checking IsHovered
	for (int32 i = 0; i < VariantButtons.Num(); ++i)
	{
		if (VariantButtons[i] && VariantButtons[i]->IsHovered())
		{
			if (VariantStyleIds.IsValidIndex(i))
			{
				SelectVariant(VariantStyleIds[i]);
			}
			return;
		}
	}
}

// ============================================================
// SelectVariant
// ============================================================
void UEditorSidebarWidget::SelectVariant(FName StyleId)
{
	CurrentVisualStyleId = StyleId;
	LastSelectedVariant.Add(CurrentBrush, StyleId);

	// Update variant border highlights
	for (int32 i = 0; i < VariantBorders.Num(); ++i)
	{
		UBorder* VBorder = VariantBorders[i];
		if (!VBorder) continue;

		if (VariantStyleIds.IsValidIndex(i) && VariantStyleIds[i] == StyleId)
		{
			VBorder->SetBrushColor(SidebarColors::VariantSelectedBorder);
		}
		else
		{
			VBorder->SetBrushColor(SidebarColors::VariantNormalBorder);
		}
	}

	OnVariantSelected.Broadcast(StyleId);
}
