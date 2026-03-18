#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GroupEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;

UCLASS(Blueprintable)
class TUIXIANGZI_API UGroupEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InGroupId, const FText& InDisplayName, FLinearColor InBaseColor);

	void SetSelected(bool bSelected);
	void SetBaseColor(FLinearColor NewColor);
	void SetInteractionEnabled(bool bEnabled);

	int32 GetGroupId() const { return GroupId; }

	// --- Delegates ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupEntryAction, int32, GroupId);

	UPROPERTY(BlueprintAssignable)
	FOnGroupEntryAction OnRowClicked;

	UPROPERTY(BlueprintAssignable)
	FOnGroupEntryAction OnColorEditClicked;

	UPROPERTY(BlueprintAssignable)
	FOnGroupEntryAction OnDeleteClicked;

protected:
	UPROPERTY(meta = (BindWidget))
	UButton* RowButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SelectionMark;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* NameText;

	UPROPERTY(meta = (BindWidget))
	UImage* ColorPreview;

	UPROPERTY(meta = (BindWidget))
	UButton* ColorEditButton;

	UPROPERTY(meta = (BindWidget))
	UButton* DeleteButton;

	int32 GroupId = 0;
	bool bIsSelected = false;

	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleRowClicked();

	UFUNCTION()
	void HandleColorEditClicked();

	UFUNCTION()
	void HandleDeleteClicked();

	UFUNCTION()
	void HandleRowHovered();

	UFUNCTION()
	void HandleRowUnhovered();
};
