#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SokobanSaveGame.generated.h"

UCLASS()
class TUIXIANGZI_API USokobanSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    /** Filenames of completed preset levels (e.g. "Level_01.json") */
    UPROPERTY()
    TSet<FString> CompletedPresetLevels;

    /** Highest unlocked preset level index (0-based). Level N+1 unlocks when level N is completed. */
    UPROPERTY()
    int32 HighestUnlockedPresetIndex = 0;

    /** Whether the editor tutorial has been shown. */
    UPROPERTY()
    bool bEditorTutorialCompleted = false;

    static const FString SaveSlotName;
};
