#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TutorialWidget.generated.h"

class UTextBlock;
class UButton;

UCLASS(Blueprintable)
class TUIXIANGZI_API UTutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTutorialAdvanced);

	UPROPERTY(BlueprintAssignable, Category = "Tutorial")
	FOnTutorialAdvanced OnAdvanced;

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SetTutorialText(const FText& Text);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SetButtonText(const FText& Text);

	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void SetNextButtonVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Tutorial")
	UTextBlock* TutorialText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Tutorial")
	UButton* NextButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Tutorial")
	UTextBlock* NextButtonText = nullptr;

private:
	UFUNCTION()
	void OnNextButtonClicked();
};
