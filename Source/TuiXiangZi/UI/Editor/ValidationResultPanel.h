#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Editor/EditorBrushTypes.h"
#include "ValidationResultPanel.generated.h"

struct FLevelValidationResult;
class UTextBlock;
class UScrollBox;
class UButton;

UCLASS(Blueprintable)
class TUIXIANGZI_API UValidationResultPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Editor|Validation")
	void Setup(const FLevelValidationResult& Result, EValidationContext Context);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnValidationAction);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Validation")
	FOnValidationAction OnForceConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Editor|Validation")
	FOnValidationAction OnClosed;

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* ResultList;

	UPROPERTY(meta = (BindWidget))
	UButton* CloseButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ForceButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ForceButtonText;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleForceClicked();

	void AddResultEntry(const FString& Message, bool bIsError);
};
