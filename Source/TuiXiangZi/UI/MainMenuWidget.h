#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UWidgetSwitcher;

UCLASS(Blueprintable)
class TUIXIANGZI_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void OpenPresetLevelSelect();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void OpenCustomLevelSelect();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void OpenLevelEditor();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void ShowMainPanel();

    UFUNCTION(BlueprintCallable, Category = "Menu")
    void QuitGame();

protected:
    /** Bind to a WidgetSwitcher in Blueprint to switch between main panel / preset select / custom select */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Menu")
    UWidgetSwitcher* ContentSwitcher;
};
