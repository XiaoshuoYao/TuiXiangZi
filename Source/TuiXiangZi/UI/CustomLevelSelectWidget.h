#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CustomLevelSelectWidget.generated.h"

class ULevelSelectEntryData;

UCLASS(Blueprintable)
class TUIXIANGZI_API UCustomLevelSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Rebuild the level list from custom levels in Saved/Levels/ */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void RefreshLevelList();

    /** Select a level entry */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void SelectLevel(ULevelSelectEntryData* EntryData);

    /** Launch the selected level */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void PlaySelectedLevel();

    /** Return to main menu panel */
    UFUNCTION(BlueprintCallable, Category = "Menu")
    void GoBack();

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCustomLevelListRefreshed);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCustomSelectionChanged);

    /** Fired after RefreshLevelList completes */
    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnCustomLevelListRefreshed OnLevelListRefreshed;

    /** Fired when a level is selected — use in Blueprint to update item visuals */
    UPROPERTY(BlueprintAssignable, Category = "Menu")
    FOnCustomSelectionChanged OnSelectionChanged;

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Menu")
    TArray<ULevelSelectEntryData*> LevelEntries;

    UPROPERTY(BlueprintReadOnly, Category = "Menu")
    ULevelSelectEntryData* SelectedEntry = nullptr;
};
