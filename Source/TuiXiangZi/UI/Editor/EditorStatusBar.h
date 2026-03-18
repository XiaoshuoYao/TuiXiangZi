#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Editor/EditorBrushTypes.h"
#include "EditorStatusBar.generated.h"

class UTextBlock;
class UWidgetAnimation;

UCLASS(Blueprintable)
class TUIXIANGZI_API UEditorStatusBar : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Public refresh interface (called by MainWidget) ---

	UFUNCTION(BlueprintCallable, Category = "Editor|StatusBar")
	void RefreshModeText(EEditorMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Editor|StatusBar")
	void RefreshBrushText(EEditorBrush Brush);

	UFUNCTION(BlueprintCallable, Category = "Editor|StatusBar")
	void RefreshStats(int32 CellCount, int32 BoxCount, int32 GroupCount);

	UFUNCTION(BlueprintCallable, Category = "Editor|StatusBar")
	void ShowTemporaryMessage(const FText& Message, float Duration = 3.0f);

protected:
	// --- BindWidget ---

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ModeText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* BrushText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatsText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TempMessageText;

	virtual void NativeConstruct() override;

	// --- Temporary message timer ---
	FTimerHandle TempMessageTimerHandle;

	UFUNCTION()
	void ClearTemporaryMessage();

	// --- Blink animation: create a Widget Animation named "BlinkAnimation" in Blueprint ---
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	UWidgetAnimation* BlinkAnimation = nullptr;

	bool bIsBlinking = false;

private:
	void StopBlinkIfNeeded();
};
