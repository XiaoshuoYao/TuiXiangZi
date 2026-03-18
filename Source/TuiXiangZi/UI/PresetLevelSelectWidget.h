#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PresetLevelSelectWidget.generated.h"

class ULevelSelectEntryData;

UCLASS(Blueprintable)
class TUIXIANGZI_API UPresetLevelSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Rebuild the level list from GameInstance preset data */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void RefreshLevelList();

    /** Select a level entry (call from Blueprint list item click) */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void SelectLevel(ULevelSelectEntryData* EntryData);

    /** Launch the selected level */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void PlaySelectedLevel();

    /** Return to main menu panel */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void GoBack();

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLevelListRefreshed);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectionChanged);

    /** Fired after RefreshLevelList completes — use in Blueprint to rebuild UI list */
    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnLevelListRefreshed OnLevelListRefreshed;

    /** Fired when a level is selected — use in Blueprint to update item visuals */
    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnSelectionChanged OnSelectionChanged;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Menu")
    TArray<ULevelSelectEntryData*> LevelEntries;

    UPROPERTY(BlueprintReadOnly, Category = "Menu")
    ULevelSelectEntryData* SelectedEntry = nullptr;
};
