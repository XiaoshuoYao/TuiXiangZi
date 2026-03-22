#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CustomLevelSelectWidget.generated.h"

class ULevelSelectEntryData;
class UScrollBox;
class UButton;
class UBorder;

UCLASS(Blueprintable)
class TUIXIANGZI_API UCustomLevelSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Rebuild the level list from custom levels in Saved/Levels/ */
    void RefreshLevelList();

protected:
    virtual void NativeConstruct() override;

    // --- BindWidget ---
    UPROPERTY(meta = (BindWidget))
    UScrollBox* LevelListScrollBox;

    UPROPERTY(meta = (BindWidget))
    UButton* PlayButton;

    UPROPERTY(meta = (BindWidget))
    UButton* BackButton;

private:
    void CreateLevelEntryWidget(ULevelSelectEntryData* EntryData, int32 Index);
    void UpdateSelectionVisuals();

    UFUNCTION()
    void HandleAnyEntryClicked();

    UFUNCTION()
    void HandlePlayClicked();

    UFUNCTION()
    void HandleBackClicked();

    UPROPERTY()
    TArray<ULevelSelectEntryData*> LevelEntries;

    UPROPERTY()
    ULevelSelectEntryData* SelectedEntry = nullptr;

    UPROPERTY()
    TArray<UButton*> EntryButtons;

    UPROPERTY()
    TArray<UBorder*> EntryBorders;
};
