#include "UI/MainMenuWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::OpenPresetLevelSelect()
{
    if (ContentSwitcher)
    {
        ContentSwitcher->SetActiveWidgetIndex(1);
    }
}

void UMainMenuWidget::OpenCustomLevelSelect()
{
    if (ContentSwitcher)
    {
        ContentSwitcher->SetActiveWidgetIndex(2);
    }
}

void UMainMenuWidget::ShowMainPanel()
{
    if (ContentSwitcher)
    {
        ContentSwitcher->SetActiveWidgetIndex(0);
    }
}

void UMainMenuWidget::OpenLevelEditor()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("EditorMap")));
}

void UMainMenuWidget::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
