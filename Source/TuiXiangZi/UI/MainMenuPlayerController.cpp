#include "UI/MainMenuPlayerController.h"
#include "UI/MainMenuWidget.h"
#include "Blueprint/UserWidget.h"

void AMainMenuPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // UI-only input mode for menu
    FInputModeUIOnly InputMode;
    SetInputMode(InputMode);
    bShowMouseCursor = true;

    // Create and display the main menu widget
    if (MainMenuWidgetClass)
    {
        MainMenuWidget = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
        if (MainMenuWidget)
        {
            MainMenuWidget->AddToViewport();
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MainMenuPlayerController: MainMenuWidgetClass is not set! "
            "Please create a Blueprint subclass and assign the widget class."));
    }
}
