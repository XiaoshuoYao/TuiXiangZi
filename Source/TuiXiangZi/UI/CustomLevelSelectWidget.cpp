#include "UI/CustomLevelSelectWidget.h"
#include "UI/LevelSelectEntryData.h"
#include "Framework/SokobanGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

void UCustomLevelSelectWidget::RefreshLevelList()
{
    LevelEntries.Empty();
    SelectedEntry = nullptr;

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    TArray<FString> CustomPaths = GI->GetCustomLevelPaths();

    for (int32 i = 0; i < CustomPaths.Num(); ++i)
    {
        ULevelSelectEntryData* Entry = NewObject<ULevelSelectEntryData>(this);
        Entry->LevelFilePath = CustomPaths[i];
        Entry->PresetIndex = -1;
        Entry->SourceType = ELevelSourceType::Custom;
        Entry->bIsUnlocked = true; // Custom levels are always unlocked
        Entry->bIsCompleted = false;

        // Extract display name from filename
        FString FileName = FPaths::GetBaseFilename(CustomPaths[i]);
        Entry->LevelName = FileName.Replace(TEXT("_"), TEXT(" "));

        LevelEntries.Add(Entry);
    }

    OnLevelListRefreshed.Broadcast();
}

void UCustomLevelSelectWidget::SelectLevel(ULevelSelectEntryData* EntryData)
{
    if (EntryData)
    {
        SelectedEntry = EntryData;
    }
}

void UCustomLevelSelectWidget::PlaySelectedLevel()
{
    if (!SelectedEntry) return;

    USokobanGameInstance* GI = Cast<USokobanGameInstance>(GetGameInstance());
    if (!GI) return;

    GI->SelectedLevelPath = SelectedEntry->LevelFilePath;
    GI->SelectedLevelSource = ELevelSourceType::Custom;
    GI->SelectedPresetIndex = -1;

    UGameplayStatics::OpenLevel(this, FName(TEXT("GameMap")));
}

void UCustomLevelSelectWidget::GoBack()
{
    SelectedEntry = nullptr;
}
