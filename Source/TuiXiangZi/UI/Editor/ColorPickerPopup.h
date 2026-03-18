#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ColorPickerPopup.generated.h"

class UImage;
class UButton;
class UEditableTextBox;
class UWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class TUIXIANGZI_API UColorPickerPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InGroupId, FLinearColor CurrentBaseColor);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnColorConfirmed,
		int32, GroupId, FLinearColor, BaseColor, FLinearColor, ActiveColor);

	UPROPERTY(BlueprintAssignable)
	FOnColorConfirmed OnColorConfirmed;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnColorCancelled);

	UPROPERTY(BlueprintAssignable)
	FOnColorCancelled OnCancelled;

protected:
	// --- BindWidget ---
	UPROPERTY(meta = (BindWidget))
	UImage* SVPlane;

	UPROPERTY(meta = (BindWidget))
	UImage* SVCursor;

	UPROPERTY(meta = (BindWidget))
	UImage* HueBar;

	UPROPERTY(meta = (BindWidget))
	UImage* HueCursor;

	UPROPERTY(meta = (BindWidget))
	UImage* OldColorPreview;

	UPROPERTY(meta = (BindWidget))
	UImage* NewColorPreview;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* HexInput;

	UPROPERTY(meta = (BindWidget))
	UButton* ApplyButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	// Material for SV plane (set in Blueprint defaults)
	UPROPERTY(EditDefaultsOnly, Category = "ColorPicker")
	UMaterialInterface* SVPlaneMaterial;

	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

	int32 GroupId = 0;
	FLinearColor OriginalColor;

	// HSV state
	float CurrentH = 0.0f;   // 0~360
	float CurrentS = 1.0f;   // 0~1
	float CurrentV = 1.0f;   // 0~1

	bool bDraggingSV = false;
	bool bDraggingHue = false;

	UPROPERTY()
	UMaterialInstanceDynamic* SVPlaneMID;

	void UpdateFromHSV();
	void UpdateCursorPositions();
	void RegenerateSVTexture();
	FLinearColor HSVToLinear(float H, float S, float V) const;
	void LinearToHSV(FLinearColor Color, float& OutH, float& OutS, float& OutV) const;
	FLinearColor CalculateActiveColor(FLinearColor Base) const;

	/** Get local coordinates within a widget given absolute position. Returns true if inside. */
	bool GetLocalHitPosition(UWidget* TargetWidget, const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent, FVector2D& OutLocal, FVector2D& OutSize) const;

	UFUNCTION()
	void HandleHexCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleCancelClicked();
};
