#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UWidgetSwitcher;
class UButton;
class UPresetLevelSelectWidget;
class UCustomLevelSelectWidget;

UCLASS(Blueprintable)
class TUIXIANGZI_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Called by child panels to return to main panel */
    void ShowMainPanel();

protected:
    virtual void NativeConstruct() override;

    // --- BindWidget: structural widgets in Blueprint ---
    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* ContentSwitcher;

    UPROPERTY(meta = (BindWidget))
    UButton* PresetPlayButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CustomPlayButton;

    UPROPERTY(meta = (BindWidget))
    UButton* EditorButton;

    UPROPERTY(meta = (BindWidget))
    UButton* QuitButton;

    UPROPERTY(meta = (BindWidget))
    UPresetLevelSelectWidget* PresetLevelSelect;

    UPROPERTY(meta = (BindWidget))
    UCustomLevelSelectWidget* CustomLevelSelect;

private:
    UFUNCTION()
    void HandlePresetPlayClicked();

    UFUNCTION()
    void HandleCustomPlayClicked();

    UFUNCTION()
    void HandleEditorClicked();

    UFUNCTION()
    void HandleQuitClicked();
};
