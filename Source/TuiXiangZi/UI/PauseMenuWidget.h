#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable)
class TUIXIANGZI_API UPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void OnResumeClicked();

    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void OnReturnToMainMenuClicked();

    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void OnRestartClicked();

    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void OnExitGameClicked();

    /** Set the title text (e.g. "Paused" or "Level Complete!") */
    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void SetTitleText(const FText& NewTitle);

    /** Show/hide the resume button (hidden when level is completed) */
    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void SetResumeButtonVisible(bool bVisible);

    /** Show/hide the next level button */
    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void SetNextLevelButtonVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "PauseMenu")
    void OnNextLevelClicked();

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu")
    UTextBlock* TitleText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu")
    UWidget* ResumeButton;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu")
    UWidget* NextLevelButton;
};
