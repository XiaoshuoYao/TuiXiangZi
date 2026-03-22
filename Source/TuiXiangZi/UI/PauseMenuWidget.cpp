#include "UI/PauseMenuWidget.h"
#include "Framework/SokobanGameMode.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleResumeClicked);
    RestartButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleRestartClicked);
    MainMenuButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleMainMenuClicked);
    NextLevelButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleNextLevelClicked);
    ExitButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::HandleExitClicked);
}

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

void UPauseMenuWidget::HandleResumeClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->TogglePauseMenu();
    }
}

void UPauseMenuWidget::HandleRestartClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->HidePauseMenu();
        GM->ResetCurrentLevel();
    }
}

void UPauseMenuWidget::HandleMainMenuClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->ReturnToMainMenu();
    }
}

void UPauseMenuWidget::HandleNextLevelClicked()
{
    ASokobanGameMode* GM = Cast<ASokobanGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->HidePauseMenu();
        GM->LoadNextLevel();
    }
}

void UPauseMenuWidget::HandleExitClicked()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
