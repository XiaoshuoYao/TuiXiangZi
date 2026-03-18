#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Editor/EditorBrushTypes.h"
#include "Grid/GridTypes.h"
#include "EditorSidebarWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UUniformGridPanel;
class UButton;
class UBorder;
class UTileStyleCatalog;

UCLASS(Blueprintable)
class TUIXIANGZI_API UEditorSidebarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Supply MainWidget calls ---
	UFUNCTION(BlueprintCallable, Category = "Editor|Sidebar")
	void SetActiveBrush(EEditorBrush Brush);

	UFUNCTION(BlueprintCallable, Category = "Editor|Sidebar")
	void SetEnabled(bool bEnabled);

	void InitializeWithCatalog(UTileStyleCatalog* Catalog);

	// --- Delegates (bound by MainWidget) ---
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrushSelected, EEditorBrush, Brush);
	UPROPERTY(BlueprintAssignable, Category = "Editor|Sidebar")
	FOnBrushSelected OnBrushSelected;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVariantSelected, FName, StyleId);
	UPROPERTY(BlueprintAssignable, Category = "Editor|Sidebar")
	FOnVariantSelected OnVariantSelected;

protected:
	// --- BindWidget ---
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* BrushButtonContainer;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* VariantPanel;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* VariantTitle;

	UPROPERTY(meta = (BindWidget))
	UUniformGridPanel* VariantGrid;

	virtual void NativeConstruct() override;

	// --- Internal state ---
	UPROPERTY()
	TArray<UButton*> BrushButtons;

	UPROPERTY()
	TArray<UBorder*> BrushButtonBorders;

	EEditorBrush CurrentBrush = EEditorBrush::Floor;

	UPROPERTY()
	UTileStyleCatalog* TileStyleCatalog = nullptr;

	TMap<EEditorBrush, FName> LastSelectedVariant;

	FName CurrentVisualStyleId = NAME_None;

	// Ordered list of brush enums matching BrushButtons indices
	TArray<EEditorBrush> BrushOrder;

	// Variant grid: maps child index -> StyleId
	TArray<FName> VariantStyleIds;

	UPROPERTY()
	TArray<UBorder*> VariantBorders;

	UPROPERTY()
	TArray<UButton*> VariantButtons;

	// --- Internal methods ---
	void CreateBrushButtons();
	void RefreshVariantPanel(EEditorBrush Brush);
	void SelectVariant(FName StyleId);

	UFUNCTION()
	void HandleBrushButton0();
	UFUNCTION()
	void HandleBrushButton1();
	UFUNCTION()
	void HandleBrushButton2();
	UFUNCTION()
	void HandleBrushButton3();
	UFUNCTION()
	void HandleBrushButton4();
	UFUNCTION()
	void HandleBrushButton5();
	UFUNCTION()
	void HandleBrushButton6();
	UFUNCTION()
	void HandleBrushButton7();
	UFUNCTION()
	void HandleBrushButton8();

	void HandleBrushButtonClicked(EEditorBrush Brush);

	UFUNCTION()
	void HandleVariantButtonClicked();

	static EGridCellType BrushToCellType(EEditorBrush Brush);
};
