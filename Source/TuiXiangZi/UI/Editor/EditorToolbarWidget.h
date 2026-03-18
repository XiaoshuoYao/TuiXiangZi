#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditorToolbarWidget.generated.h"

class UButton;

UCLASS(Blueprintable)
class TUIXIANGZI_API UEditorToolbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Delegates (for MainWidget to bind) ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToolbarAction);

	UPROPERTY(BlueprintAssignable, Category = "Editor|Toolbar")
	FOnToolbarAction OnNewClicked;

	UPROPERTY(BlueprintAssignable, Category = "Editor|Toolbar")
	FOnToolbarAction OnSaveClicked;

	UPROPERTY(BlueprintAssignable, Category = "Editor|Toolbar")
	FOnToolbarAction OnLoadClicked;

	UPROPERTY(BlueprintAssignable, Category = "Editor|Toolbar")
	FOnToolbarAction OnTestClicked;

	UPROPERTY(BlueprintAssignable, Category = "Editor|Toolbar")
	FOnToolbarAction OnBackClicked;

protected:
	// --- BindWidget ---
	UPROPERTY(meta = (BindWidget))
	UButton* NewButton;

	UPROPERTY(meta = (BindWidget))
	UButton* SaveButton;

	UPROPERTY(meta = (BindWidget))
	UButton* LoadButton;

	UPROPERTY(meta = (BindWidget))
	UButton* TestButton;

	UPROPERTY(meta = (BindWidget))
	UButton* BackButton;

	// --- Internal ---
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleNewClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleTestClicked();

	UFUNCTION()
	void HandleBackClicked();
};
