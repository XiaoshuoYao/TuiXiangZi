#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConfirmDialog.generated.h"

class UTextBlock;
class UButton;

UCLASS(Blueprintable)
class TUIXIANGZI_API UConfirmDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Public Interface ---
	UFUNCTION(BlueprintCallable, Category = "Editor|Dialog")
	void Setup(const FText& Title, const FText& Message,
	           const FText& ConfirmText = NSLOCTEXT("Editor", "OK", "确定"),
	           const FText& CancelText = NSLOCTEXT("Editor", "Cancel", "取消"));

	// --- Delegates ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogResult);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnDialogResult OnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnDialogResult OnCancelled;

protected:
	// --- BindWidget ---
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MessageText;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ConfirmButtonText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CancelButtonText;

	// --- Internal ---
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
