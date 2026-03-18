#include "UI/PresetLevelSelectWidget.h"
#include "UI/LevelSelectEntryData.h"
#include "Framework/SokobanGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

void UPresetLevelSelectWidget::RefreshLevelList()
{
    LevelEntries.Empty();
    SelectedEntry = nullptr;

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

        // Extract display name from filename: "Level_01.json" -> "Level 01"
        FString FileName = FPaths::GetBaseFilename(PresetPaths[i]);
        Entry->LevelName = FileName.Replace(TEXT("_"), TEXT(" "));

        Entry->bIsCompleted = GI->IsPresetLevelCompleted(FPaths::GetCleanFilename(PresetPaths[i]));

        LevelEntries.Add(Entry);
    }

    OnLevelListRefreshed.Broadcast();
}

void UPresetLevelSelectWidget::SelectLevel(ULevelSelectEntryData* EntryData)
{
    if (EntryData && EntryData->bIsUnlocked)
    {
        SelectedEntry = EntryData;
    }
}

void UPresetLevelSelectWidget::PlaySelectedLevel()
{
    if (!SelectedEntry || !SelectedEntry->bIsUnlocked) return;

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    GI->SelectedLevelPath = SelectedEntry->LevelFilePath;
    GI->SelectedLevelSource = ELevelSourceType::Preset;
    GI->SelectedPresetIndex = SelectedEntry->PresetIndex;

    UGameplayStatics::OpenLevel(this, FName(TEXT("GameMap")));
}

void UPresetLevelSelectWidget::GoBack()
{
    // Find the parent MainMenuWidget and switch back to main panel
    // Blueprint should call MainMenuWidget->ShowMainPanel() directly
    // This is a convenience for Blueprint to override
    SelectedEntry = nullptr;
}
