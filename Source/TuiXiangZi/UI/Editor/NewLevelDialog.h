#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NewLevelDialog.generated.h"

class USpinBox;
class UTextBlock;
class UButton;

UCLASS(Blueprintable)
class TUIXIANGZI_API UNewLevelDialog : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNewLevelConfirmed, int32, Width, int32, Height);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnNewLevelConfirmed OnConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewLevelCancelled);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Dialog")
	FOnNewLevelCancelled OnCancelled;

protected:
	UPROPERTY(meta = (BindWidget))
	USpinBox* WidthSpinBox;

	UPROPERTY(meta = (BindWidget))
	USpinBox* HeightSpinBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PreviewText;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleWidthChanged(float Value);

	UFUNCTION()
	void HandleHeightChanged(float Value);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	void UpdatePreview();
};
