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

	const FLevelTutorialData* FindTutorialForLevel(int32 PresetLevelIndex) const;
};
