#include "UI/PresetLevelSelectWidget.h"
#include "UI/LevelSelectEntryData.h"
#include "UI/MainMenuWidget.h"
#include "Framework/SokobanGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

// ============================================================
// Color constants
// ============================================================
namespace LevelSelectColors
{
    static const FLinearColor SelectedBorder(0.267f, 0.6f, 1.0f, 1.0f);
    static const FLinearColor SelectedBackground(0.1f, 0.2f, 0.333f, 0.5f);
    static const FLinearColor NormalBackground(0.15f, 0.15f, 0.15f, 1.0f);
    static const FLinearColor NormalBorder(0.0f, 0.0f, 0.0f, 0.0f);
    static const FLinearColor LockedText(0.4f, 0.4f, 0.4f, 1.0f);
    static const FLinearColor NormalText(1.0f, 1.0f, 1.0f, 1.0f);
    static const FLinearColor CompletedMark(0.3f, 0.85f, 0.3f, 1.0f);
}

// ============================================================
// NativeConstruct
// ============================================================
void UPresetLevelSelectWidget::NativeConstruct()
{
    Super::NativeConstruct();

    PlayButton->OnClicked.AddDynamic(this, &UPresetLevelSelectWidget::HandlePlayClicked);
    BackButton->OnClicked.AddDynamic(this, &UPresetLevelSelectWidget::HandleBackClicked);
}

// ============================================================
// RefreshLevelList
// ============================================================
void UPresetLevelSelectWidget::RefreshLevelList()
{
    LevelEntries.Empty();
    EntryButtons.Empty();
    EntryBorders.Empty();
    SelectedEntry = nullptr;
    LevelListScrollBox->ClearChildren();

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    TArray<FString> PresetPaths = GI->GetPresetLevelPaths();

    for (int32 i = 0; i < PresetPaths.Num(); ++i)
    {
        ULevelSelectEntryData* Entry = NewObject<ULevelSelectEntryData>(this);
        Entry->LevelFilePath = PresetPaths[i];
        Entry->PresetIndex = i;
        Entry->SourceType = ELevelSourceType::Preset;
        Entry->bIsUnlocked = GI->IsPresetLevelUnlocked(i);

        FString FileName = FPaths::GetBaseFilename(PresetPaths[i]);
        Entry->LevelName = FileName.Replace(TEXT("_"), TEXT(" "));

        Entry->bIsCompleted = GI->IsPresetLevelCompleted(FPaths::GetCleanFilename(PresetPaths[i]));

        LevelEntries.Add(Entry);
        CreateLevelEntryWidget(Entry, i);
    }
}

// ============================================================
// CreateLevelEntryWidget
// ============================================================
void UPresetLevelSelectWidget::CreateLevelEntryWidget(ULevelSelectEntryData* EntryData, int32 Index)
{
    // Border (selection highlight frame)
    UBorder* Border = NewObject<UBorder>(this);
    Border->SetBrushColor(LevelSelectColors::NormalBorder);

    FSlateBrush BackBrush;
    BackBrush.TintColor = FSlateColor(LevelSelectColors::NormalBackground);
    Border->SetBrush(BackBrush);
    Border->SetPadding(FMargin(8.0f, 4.0f));

    // Button (transparent style)
    UButton* Button = NewObject<UButton>(this);
    FButtonStyle BtnStyle;
    BtnStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
    BtnStyle.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
    BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
    BtnStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
    Button->SetStyle(BtnStyle);

    // HorizontalBox layout: [CompletedMark] [LevelName] [LockedIcon]
    UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);

    // Completed mark
    UTextBlock* CompletedText = NewObject<UTextBlock>(this);
    if (EntryData->bIsCompleted)
    {
        CompletedText->SetText(FText::FromString(TEXT("\u2713 ")));
        CompletedText->SetColorAndOpacity(FSlateColor(LevelSelectColors::CompletedMark));
    }
    else
    {
        CompletedText->SetText(FText::FromString(TEXT("   ")));
    }
    FSlateFontInfo MarkFont = CompletedText->GetFont();
    MarkFont.Size = 14;
    CompletedText->SetFont(MarkFont);
    UHorizontalBoxSlot* MarkSlot = HBox->AddChildToHorizontalBox(CompletedText);
    MarkSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));

    // Level name
    UTextBlock* NameText = NewObject<UTextBlock>(this);
    NameText->SetText(FText::FromString(EntryData->LevelName));
    FSlateFontInfo NameFont = NameText->GetFont();
    NameFont.Size = 16;
    NameText->SetFont(NameFont);

    FLinearColor TextColor = EntryData->bIsUnlocked
        ? LevelSelectColors::NormalText
        : LevelSelectColors::LockedText;
    NameText->SetColorAndOpacity(FSlateColor(TextColor));

    UHorizontalBoxSlot* NameSlot = HBox->AddChildToHorizontalBox(NameText);
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    NameSlot->SetVerticalAlignment(VAlign_Center);

    // Lock icon for locked levels
    if (!EntryData->bIsUnlocked)
    {
        UTextBlock* LockText = NewObject<UTextBlock>(this);
        LockText->SetText(FText::FromString(TEXT("\U0001F512")));
        FSlateFontInfo LockFont = LockText->GetFont();
        LockFont.Size = 12;
        LockText->SetFont(LockFont);
        LockText->SetColorAndOpacity(FSlateColor(LevelSelectColors::LockedText));
        UHorizontalBoxSlot* LockSlot = HBox->AddChildToHorizontalBox(LockText);
        LockSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
        LockSlot->SetVerticalAlignment(VAlign_Center);
    }

    Button->AddChild(HBox);
    Border->AddChild(Button);

    LevelListScrollBox->AddChild(Border);

    EntryButtons.Add(Button);
    EntryBorders.Add(Border);

    Button->OnClicked.AddDynamic(this, &UPresetLevelSelectWidget::HandleAnyEntryClicked);
}

// ============================================================
// Selection
// ============================================================
void UPresetLevelSelectWidget::HandleAnyEntryClicked()
{
    for (int32 i = 0; i < EntryButtons.Num(); ++i)
    {
        if (EntryButtons[i] && EntryButtons[i]->IsHovered())
        {
            if (LevelEntries.IsValidIndex(i) && LevelEntries[i]->bIsUnlocked)
            {
                if (SelectedEntry)
                {
                    SelectedEntry->bIsSelected = false;
                }

                SelectedEntry = LevelEntries[i];
                SelectedEntry->bIsSelected = true;
                UpdateSelectionVisuals();
            }
            return;
        }
    }
}

void UPresetLevelSelectWidget::HandleAnyEntryDoubleClicked()
{
    // Double-click to play
    if (SelectedEntry && SelectedEntry->bIsUnlocked)
    {
        HandlePlayClicked();
    }
}

void UPresetLevelSelectWidget::UpdateSelectionVisuals()
{
    for (int32 i = 0; i < EntryBorders.Num(); ++i)
    {
        if (!EntryBorders[i]) continue;

        bool bSelected = LevelEntries.IsValidIndex(i) && LevelEntries[i]->bIsSelected;

        EntryBorders[i]->SetBrushColor(
            bSelected ? LevelSelectColors::SelectedBorder : LevelSelectColors::NormalBorder);

        FSlateBrush Bg;
        Bg.TintColor = FSlateColor(
            bSelected ? LevelSelectColors::SelectedBackground : LevelSelectColors::NormalBackground);
        EntryBorders[i]->SetBrush(Bg);
    }
}

// ============================================================
// Play / Back
// ============================================================
void UPresetLevelSelectWidget::HandlePlayClicked()
{
    if (!SelectedEntry || !SelectedEntry->bIsUnlocked) return;

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    GI->SelectedLevelPath = SelectedEntry->LevelFilePath;
    GI->SelectedLevelSource = ELevelSourceType::Preset;
    GI->SelectedPresetIndex = SelectedEntry->PresetIndex;

    UGameplayStatics::OpenLevel(this, FName(TEXT("GameMap")));
}

void UPresetLevelSelectWidget::HandleBackClicked()
{
    SelectedEntry = nullptr;

    if (UMainMenuWidget* Menu = GetTypedOuter<UMainMenuWidget>())
    {
        Menu->ShowMainPanel();
    }
}
