#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UTextBlock;
class UButton;

UCLASS(Blueprintable)
class TUIXIANGZI_API UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Set the title text (e.g. "Paused" or "Level Complete!") */
    void SetTitleText(const FText& NewTitle);

    /** Show/hide the resume button (hidden when level is completed) */
    void SetResumeButtonVisible(bool bVisible);

    /** Show/hide the next level button */
    void SetNextLevelButtonVisible(bool bVisible);

protected:
    virtual void NativeConstruct() override;

    // --- BindWidget ---
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    UButton* ResumeButton;

    UPROPERTY(meta = (BindWidget))
    UButton* RestartButton;

    UPROPERTY(meta = (BindWidget))
    UButton* MainMenuButton;

    UPROPERTY(meta = (BindWidget))
    UButton* NextLevelButton;

    UPROPERTY(meta = (BindWidget))
    UButton* ExitButton;

private:
    UFUNCTION()
    void HandleResumeClicked();

    UFUNCTION()
    void HandleRestartClicked();

    UFUNCTION()
    void HandleMainMenuClicked();

    UFUNCTION()
    void HandleNextLevelClicked();

    UFUNCTION()
    void HandleExitClicked();
};
