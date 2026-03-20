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

// ============================================================
// Brush configuration table
// ============================================================
struct FBrushConfig
{
	EEditorBrush Brush;
	const TCHAR* DisplayName;
	const TCHAR* Shortcut;
	FLinearColor IconColor;
};

static const FBrushConfig GBrushConfigs[] =
{
	{ EEditorBrush::Floor,         TEXT("地板"),     TEXT("1"), FLinearColor(1.0f, 1.0f, 1.0f) },
	{ EEditorBrush::Wall,          TEXT("墙壁"),     TEXT("2"), FLinearColor(0.533f, 0.533f, 0.533f) },
	{ EEditorBrush::Ice,           TEXT("冰面"),     TEXT("3"), FLinearColor(0.4f, 0.8f, 1.0f) },
	{ EEditorBrush::Goal,          TEXT("目标"),     TEXT("4"), FLinearColor(1.0f, 0.267f, 0.267f) },
	{ EEditorBrush::Door,          TEXT("机关门"),   TEXT("5"), FLinearColor(0.667f, 0.4f, 1.0f) },
	{ EEditorBrush::PressurePlate, TEXT("压力板"),   TEXT("6"), FLinearColor(1.0f, 0.533f, 0.0f) },
	{ EEditorBrush::BoxSpawn,      TEXT("箱子生成"), TEXT("7"), FLinearColor(1.0f, 0.667f, 0.0f) },
	{ EEditorBrush::PlayerStart,   TEXT("玩家起点"), TEXT("8"), FLinearColor(0.267f, 1.0f, 0.267f) },
	{ EEditorBrush::Eraser,        TEXT("橡皮擦"),   TEXT("E"), FLinearColor(1.0f, 0.267f, 0.267f) },
};

static constexpr int32 GBrushCount = UE_ARRAY_COUNT(GBrushConfigs);

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

	for (int32 i = 0; i < GBrushCount; ++i)
	{
		const FBrushConfig& Cfg = GBrushConfigs[i];

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

	// Bind click handlers individually (AddDynamic macro requires literal function name)
	if (BrushButtons.Num() > 0 && BrushButtons[0]) BrushButtons[0]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton0);
	if (BrushButtons.Num() > 1 && BrushButtons[1]) BrushButtons[1]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton1);
	if (BrushButtons.Num() > 2 && BrushButtons[2]) BrushButtons[2]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton2);
	if (BrushButtons.Num() > 3 && BrushButtons[3]) BrushButtons[3]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton3);
	if (BrushButtons.Num() > 4 && BrushButtons[4]) BrushButtons[4]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton4);
	if (BrushButtons.Num() > 5 && BrushButtons[5]) BrushButtons[5]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton5);
	if (BrushButtons.Num() > 6 && BrushButtons[6]) BrushButtons[6]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton6);
	if (BrushButtons.Num() > 7 && BrushButtons[7]) BrushButtons[7]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton7);
	if (BrushButtons.Num() > 8 && BrushButtons[8]) BrushButtons[8]->OnClicked.AddDynamic(this, &UEditorSidebarWidget::HandleBrushButton8);
}

// ============================================================
// Per-button click handlers (UE requires parameterless UFUNCTION for OnClicked)
// ============================================================
void UEditorSidebarWidget::HandleBrushButton0() { HandleBrushButtonClicked(GBrushConfigs[0].Brush); }
void UEditorSidebarWidget::HandleBrushButton1() { HandleBrushButtonClicked(GBrushConfigs[1].Brush); }
void UEditorSidebarWidget::HandleBrushButton2() { HandleBrushButtonClicked(GBrushConfigs[2].Brush); }
void UEditorSidebarWidget::HandleBrushButton3() { HandleBrushButtonClicked(GBrushConfigs[3].Brush); }
void UEditorSidebarWidget::HandleBrushButton4() { HandleBrushButtonClicked(GBrushConfigs[4].Brush); }
void UEditorSidebarWidget::HandleBrushButton5() { HandleBrushButtonClicked(GBrushConfigs[5].Brush); }
void UEditorSidebarWidget::HandleBrushButton6() { HandleBrushButtonClicked(GBrushConfigs[6].Brush); }
void UEditorSidebarWidget::HandleBrushButton7() { HandleBrushButtonClicked(GBrushConfigs[7].Brush); }
void UEditorSidebarWidget::HandleBrushButton8() { HandleBrushButtonClicked(GBrushConfigs[8].Brush); }

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
	// Disable brush buttons so user can't switch brush
	for (UButton* Btn : BrushButtons)
	{
		if (Btn)
		{
			Btn->SetIsEnabled(false);
		}
	}

	// Show pressure plate variants
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
	switch (Brush)
	{
		case EEditorBrush::Floor:         return EGridCellType::Floor;
		case EEditorBrush::Wall:          return EGridCellType::Wall;
		case EEditorBrush::Ice:           return EGridCellType::Ice;
		case EEditorBrush::Goal:          return EGridCellType::Goal;
		case EEditorBrush::Door:          return EGridCellType::Door;
		case EEditorBrush::PressurePlate: return EGridCellType::PressurePlate;
		default:                          return EGridCellType::Empty;
	}
}

// ============================================================
// RefreshVariantPanel
// ============================================================
void UEditorSidebarWidget::RefreshVariantPanel(EEditorBrush Brush)
{
	if (!VariantPanel || !VariantGrid) return;

	EGridCellType CellType = BrushToCellType(Brush);

	if (CellType == EGridCellType::Empty || !TileStyleCatalog)
	{
		VariantPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TArray<const FTileVisualStyle*> Styles = TileStyleCatalog->GetStylesForType(CellType);

	if (Styles.Num() == 0)
	{
		VariantPanel->SetVisibility(ESlateVisibility::Collapsed);
		CurrentVisualStyleId = NAME_None;
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
