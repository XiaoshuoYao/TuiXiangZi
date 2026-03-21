#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Tutorial/TutorialTypes.h"
#include "TutorialDataAsset.generated.h"

UCLASS(BlueprintType)
class TUIXIANGZI_API UTutorialDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	TArray<FLevelTutorialData> LevelTutorials;

	/** Tutorial steps for the level editor (not tied to a specific level). */
	UPROPERTY(EditAnywhere, Category = "Tutorial|Editor")
	TArray<FTutorialStep> EditorTutorialSteps;

	const FLevelTutorialData* FindTutorialForLevel(int32 PresetLevelIndex) const;
};
