#include "UI/MainMenuWidget.h"
#include "UI/PresetLevelSelectWidget.h"
#include "UI/CustomLevelSelectWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    PresetPlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandlePresetPlayClicked);
    CustomPlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCustomPlayClicked);
    EditorButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleEditorClicked);
    QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);
}

void UMainMenuWidget::ShowMainPanel()
{
    ContentSwitcher->SetActiveWidgetIndex(0);
}

void UMainMenuWidget::HandlePresetPlayClicked()
{
    ContentSwitcher->SetActiveWidgetIndex(1);
    if (PresetLevelSelect)
    {
        PresetLevelSelect->RefreshLevelList();
    }
}

void UMainMenuWidget::HandleCustomPlayClicked()
{
    ContentSwitcher->SetActiveWidgetIndex(2);
    if (CustomLevelSelect)
    {
        CustomLevelSelect->RefreshLevelList();
    }
}

void UMainMenuWidget::HandleEditorClicked()
{
    UGameplayStatics::OpenLevel(this, FName(TEXT("EditorMap")));
}

void UMainMenuWidget::HandleQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}
