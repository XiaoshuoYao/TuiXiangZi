#include "TuiXiangZiGameMode.h"
#include "UI/MainMenuPlayerController.h"

ATuiXiangZiGameMode::ATuiXiangZiGameMode()
{
    DefaultPawnClass = nullptr; // No pawn needed in menu
    PlayerControllerClass = AMainMenuPlayerController::StaticClass();
}
