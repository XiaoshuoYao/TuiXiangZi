#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "SaveLevelDialog.generated.h"

class UTextBlock;
class UButton;
class UScrollBox;
class UEditableTextBox;

UCLASS(Blueprintable)
class TUIXIANGZI_API USaveLevelDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Editor|Dialog")
	void Setup();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveLevelConfirmed, const FString&, FileName);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnSaveLevelConfirmed OnConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveLevelCancelled);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnSaveLevelCancelled OnCancelled;

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* FileNameInput;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* ExistingLevelScrollBox;

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
	void HandleFileNameChanged(const FText& Text);

	UFUNCTION()
	void HandleExistingEntryClicked();

private:
	void PopulateExistingLevels();
	void UpdateConfirmButton();

	UPROPERTY()
	TArray<UButton*> EntryButtons;

	UPROPERTY()
	TArray<UBorder*> EntryBorders;

	TArray<FString> ExistingFileNames;
	int32 SelectedExistingIndex = -1;
};
