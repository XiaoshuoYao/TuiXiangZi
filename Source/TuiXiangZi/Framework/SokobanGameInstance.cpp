#include "Framework/SokobanGameInstance.h"
#include "Framework/SokobanSaveGame.h"
#include "LevelData/LevelSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void USokobanGameInstance::Init()
{
    Super::Init();
    LoadProgress();
}

void USokobanGameInstance::LoadProgress()
{
    USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(USokobanSaveGame::SaveSlotName, 0);
    if (Loaded)
    {
        CurrentSave = Cast<USokobanSaveGame>(Loaded);
    }

    if (!CurrentSave)
    {
        CurrentSave = Cast<USokobanSaveGame>(
            UGameplayStatics::CreateSaveGameObject(USokobanSaveGame::StaticClass()));
    }
}

void USokobanGameInstance::SaveProgress()
{
    if (CurrentSave)
    {
        UGameplayStatics::SaveGameToSlot(CurrentSave, USokobanSaveGame::SaveSlotName, 0);
    }
}

void USokobanGameInstance::MarkPresetLevelCompleted(const FString& LevelFileName)
{
    if (!CurrentSave) return;

    CurrentSave->CompletedPresetLevels.Add(LevelFileName);

    // Unlock next level if this is the current highest
    TArray<FString> PresetPaths = GetPresetLevelPaths();
    for (int32 i = 0; i < PresetPaths.Num(); ++i)
    {
        FString FileName = FPaths::GetCleanFilename(PresetPaths[i]);
        if (FileName == LevelFileName && i >= CurrentSave->HighestUnlockedPresetIndex)
        {
            CurrentSave->HighestUnlockedPresetIndex = i + 1;
            break;
        }
    }

    SaveProgress();
}

bool USokobanGameInstance::IsPresetLevelUnlocked(int32 PresetIndex) const
{
    if (!CurrentSave) return PresetIndex == 0;
    return PresetIndex <= CurrentSave->HighestUnlockedPresetIndex;
}

int32 USokobanGameInstance::GetHighestUnlockedPresetIndex() const
{
    if (!CurrentSave) return 0;
    return CurrentSave->HighestUnlockedPresetIndex;
}

bool USokobanGameInstance::IsPresetLevelCompleted(const FString& LevelFileName) const
{
    if (!CurrentSave) return false;
    return CurrentSave->CompletedPresetLevels.Contains(LevelFileName);
}

FString USokobanGameInstance::GetPresetLevelDirectory()
{
    // Primary: Content/Levels/Presets (works in both editor and packaged builds
    // when DirectoriesToAlwaysStageAsNonUFS includes Content/Levels/Presets)
    FString ContentPath = FPaths::ProjectContentDir() / TEXT("Levels") / TEXT("Presets");

    if (!FPaths::DirectoryExists(ContentPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Preset level directory not found at: %s"), *ContentPath);

        // Fallback: check relative to ProjectDir
        FString AltPath = FPaths::ProjectDir() / TEXT("Content") / TEXT("Levels") / TEXT("Presets");
        if (FPaths::DirectoryExists(AltPath))
        {
            UE_LOG(LogTemp, Log, TEXT("Found preset levels at fallback path: %s"), *AltPath);
            return AltPath;
        }
        UE_LOG(LogTemp, Warning, TEXT("Preset level fallback also not found at: %s"), *AltPath);
    }

    return ContentPath;
}

TArray<FString> USokobanGameInstance::GetPresetLevelPaths() const
{
    TArray<FString> Result;
    FString PresetDir = GetPresetLevelDirectory();

    TArray<FString> FileNames;
    IFileManager::Get().FindFiles(FileNames, *(PresetDir / TEXT("*.json")), true, false);
    FileNames.Sort();

    for (const FString& FileName : FileNames)
    {
        Result.Add(PresetDir / FileName);
    }

    return Result;
}

TArray<FString> USokobanGameInstance::GetCustomLevelPaths() const
{
    TArray<FString> Result;
    FString LevelDir = ULevelSerializer::GetDefaultLevelDirectory();

    TArray<FString> FileNames;
    ULevelSerializer::GetAvailableLevelFiles(FileNames);
    FileNames.Sort();

    for (const FString& FileName : FileNames)
    {
        Result.Add(LevelDir / FileName);
    }

    return Result;
}
