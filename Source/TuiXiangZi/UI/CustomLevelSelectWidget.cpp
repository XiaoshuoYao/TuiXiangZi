#include "UI/CustomLevelSelectWidget.h"
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

// Reuse same color scheme as preset level select
namespace CustomLevelSelectColors
{
    static const FLinearColor SelectedBorder(0.267f, 0.6f, 1.0f, 1.0f);
    static const FLinearColor SelectedBackground(0.1f, 0.2f, 0.333f, 0.5f);
    static const FLinearColor NormalBackground(0.15f, 0.15f, 0.15f, 1.0f);
    static const FLinearColor NormalBorder(0.0f, 0.0f, 0.0f, 0.0f);
    static const FLinearColor NormalText(1.0f, 1.0f, 1.0f, 1.0f);
}

// ============================================================
// NativeConstruct
// ============================================================
void UCustomLevelSelectWidget::NativeConstruct()
{
    Super::NativeConstruct();

    PlayButton->OnClicked.AddDynamic(this, &UCustomLevelSelectWidget::HandlePlayClicked);
    BackButton->OnClicked.AddDynamic(this, &UCustomLevelSelectWidget::HandleBackClicked);
}

// ============================================================
// RefreshLevelList
// ============================================================
void UCustomLevelSelectWidget::RefreshLevelList()
{
    LevelEntries.Empty();
    EntryButtons.Empty();
    EntryBorders.Empty();
    SelectedEntry = nullptr;
    LevelListScrollBox->ClearChildren();

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    TArray<FString> CustomPaths = GI->GetCustomLevelPaths();

    for (int32 i = 0; i < CustomPaths.Num(); ++i)
    {
        ULevelSelectEntryData* Entry = NewObject<ULevelSelectEntryData>(this);
        Entry->LevelFilePath = CustomPaths[i];
        Entry->PresetIndex = -1;
        Entry->SourceType = ELevelSourceType::Custom;
        Entry->bIsUnlocked = true;
        Entry->bIsCompleted = false;

        FString FileName = FPaths::GetBaseFilename(CustomPaths[i]);
        Entry->LevelName = FileName.Replace(TEXT("_"), TEXT(" "));

        LevelEntries.Add(Entry);
        CreateLevelEntryWidget(Entry, i);
    }
}

// ============================================================
// CreateLevelEntryWidget
// ============================================================
void UCustomLevelSelectWidget::CreateLevelEntryWidget(ULevelSelectEntryData* EntryData, int32 Index)
{
    UBorder* Border = NewObject<UBorder>(this);
    Border->SetBrushColor(CustomLevelSelectColors::NormalBorder);

    FSlateBrush BackBrush;
    BackBrush.TintColor = FSlateColor(CustomLevelSelectColors::NormalBackground);
    Border->SetBrush(BackBrush);
    Border->SetPadding(FMargin(8.0f, 4.0f));

    UButton* Button = NewObject<UButton>(this);
    FButtonStyle BtnStyle;
    BtnStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
    BtnStyle.Hovered.DrawAs = ESlateBrushDrawType::RoundedBox;
    BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f));
    BtnStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
    Button->SetStyle(BtnStyle);

    // HorizontalBox: [LevelName]
    UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);

    UTextBlock* NameText = NewObject<UTextBlock>(this);
    NameText->SetText(FText::FromString(EntryData->LevelName));
    FSlateFontInfo NameFont = NameText->GetFont();
    NameFont.Size = 16;
    NameText->SetFont(NameFont);
    NameText->SetColorAndOpacity(FSlateColor(CustomLevelSelectColors::NormalText));

    UHorizontalBoxSlot* NameSlot = HBox->AddChildToHorizontalBox(NameText);
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    NameSlot->SetVerticalAlignment(VAlign_Center);

    Button->AddChild(HBox);
    Border->AddChild(Button);

    LevelListScrollBox->AddChild(Border);

    EntryButtons.Add(Button);
    EntryBorders.Add(Border);

    Button->OnClicked.AddDynamic(this, &UCustomLevelSelectWidget::HandleAnyEntryClicked);
}

// ============================================================
// Selection
// ============================================================
void UCustomLevelSelectWidget::HandleAnyEntryClicked()
{
    for (int32 i = 0; i < EntryButtons.Num(); ++i)
    {
        if (EntryButtons[i] && EntryButtons[i]->IsHovered())
        {
            if (LevelEntries.IsValidIndex(i))
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

void UCustomLevelSelectWidget::UpdateSelectionVisuals()
{
    for (int32 i = 0; i < EntryBorders.Num(); ++i)
    {
        if (!EntryBorders[i]) continue;

        bool bSelected = LevelEntries.IsValidIndex(i) && LevelEntries[i]->bIsSelected;

        EntryBorders[i]->SetBrushColor(
            bSelected ? CustomLevelSelectColors::SelectedBorder : CustomLevelSelectColors::NormalBorder);

        FSlateBrush Bg;
        Bg.TintColor = FSlateColor(
            bSelected ? CustomLevelSelectColors::SelectedBackground : CustomLevelSelectColors::NormalBackground);
        EntryBorders[i]->SetBrush(Bg);
    }
}

// ============================================================
// Play / Back
// ============================================================
void UCustomLevelSelectWidget::HandlePlayClicked()
{
    if (!SelectedEntry) return;

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    GI->SelectedLevelPath = SelectedEntry->LevelFilePath;
    GI->SelectedLevelSource = ELevelSourceType::Custom;
    GI->SelectedPresetIndex = -1;

    UGameplayStatics::OpenLevel(this, FName(TEXT("GameMap")));
}

void UCustomLevelSelectWidget::HandleBackClicked()
{
    SelectedEntry = nullptr;

    if (UMainMenuWidget* Menu = GetTypedOuter<UMainMenuWidget>())
    {
        Menu->ShowMainPanel();
    }
}
