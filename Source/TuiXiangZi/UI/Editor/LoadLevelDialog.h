#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "LoadLevelDialog.generated.h"

class UTextBlock;
class UButton;
class UScrollBox;

UCLASS(Blueprintable)
class TUIXIANGZI_API ULoadLevelDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Editor|Dialog")
	void Setup();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadLevelConfirmed, const FString&, FileName);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnLoadLevelConfirmed OnConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadLevelCancelled);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnLoadLevelCancelled OnCancelled;

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* LevelListScrollBox;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ConfirmButtonText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* EmptyHintText;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleEntryClicked();

private:
	void PopulateLevelList();
	void SelectEntry(int32 Index);

	UPROPERTY()
	TArray<UButton*> EntryButtons;

	UPROPERTY()
	TArray<UBorder*> EntryBorders;

	TArray<FString> LevelFileNames;
	int32 SelectedIndex = -1;
};
