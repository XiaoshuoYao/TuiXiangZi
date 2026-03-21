#include "Tutorial/TutorialDataAsset.h"

const FLevelTutorialData* UTutorialDataAsset::FindTutorialForLevel(int32 PresetLevelIndex) const
{
	for (const FLevelTutorialData& LevelTutorial : LevelTutorials)
	{
		if (LevelTutorial.PresetLevelIndex == PresetLevelIndex)
		{
			return &LevelTutorial;
		}
	}
	return nullptr;
}
