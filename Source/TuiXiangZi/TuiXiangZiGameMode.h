#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TuiXiangZiGameMode.generated.h"

/**
 * Main Menu GameMode — used on MainMenuMap.
 * Sets up UI-only controller with no pawn.
 */
UCLASS(minimalapi)
class ATuiXiangZiGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ATuiXiangZiGameMode();
};
