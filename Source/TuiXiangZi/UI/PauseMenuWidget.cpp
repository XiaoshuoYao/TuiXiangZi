#include "UI/PauseMenuWidget.h"
#include "Framework/SokobanGameMode.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UPauseMenuWidget::SetTitleText(const FText& NewTitle)
{
    if (TitleText)
    {
        TitleText->SetText(NewTitle);
    }
}

void UPauseMenuWidget::SetResumeButtonVisible(bool bVisible)
{
    if (ResumeButton)
    {
        ResumeButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPauseMenuWidget::SetNextLevelButtonVisible(bool bVisible)
{
    if (NextLevelButton)
    {
        NextLevelButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPauseMenuWidget::OnResumeClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->TogglePauseMenu();
    }
}

void UPauseMenuWidget::OnReturnToMainMenuClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->ReturnToMainMenu();
    }
}

void UPauseMenuWidget::OnRestartClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->HidePauseMenu();
        GM->ResetCurrentLevel();
    }
}

void UPauseMenuWidget::OnExitGameClicked()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UPauseMenuWidget::OnNextLevelClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->HidePauseMenu();
        GM->LoadNextLevel();
    }
}
