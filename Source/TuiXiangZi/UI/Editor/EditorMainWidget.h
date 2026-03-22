#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Editor/EditorBrushTypes.h"
#include "EditorMainWidget.generated.h"

class UCanvasPanel;
class UButton;
class UEditorSidebarWidget;
class UEditorToolbarWidget;
class UGroupManagerPanel;
class UEditorStatusBar;
class UConfirmDialog;
class UNewLevelDialog;
class UValidationResultPanel;
class UColorPickerPopup;
class ULoadLevelDialog;
class USaveLevelDialog;
class ALevelEditorGameMode;
struct FLevelValidationResult;

UCLASS(Blueprintable)
class TUIXIANGZI_API UEditorMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Query whether a dialog is currently open (used by Pawn to block viewport input). */
	UFUNCTION(BlueprintCallable, Category = "Editor|Main")
	bool IsDialogOpen() const { return bIsDialogOpen; }

	/** Called by Pawn on Esc key press. Returns true if input was consumed. */
	UFUNCTION(BlueprintCallable, Category = "Editor|Main")
	bool HandleEscPressed();

	/** Called by Pawn to request erase confirmation at a grid position. */
	UFUNCTION(BlueprintCallable, Category = "Editor|Main")
	void RequestEraseConfirm(FIntPoint GridPos);

	/** Called by Pawn hotkeys to set a brush directly. */
	UFUNCTION(BlueprintCallable, Category = "Editor|Main")
	void RequestSetBrush(EEditorBrush Brush);

	/** Called by Pawn hotkeys for toolbar actions. */
	UFUNCTION(BlueprintCallable, Category = "Editor|Main")
	void RequestToolbarAction(EToolbarAction Action);

	/** Refresh status bar stats (call after each Paint/Erase). */
	UFUNCTION(BlueprintCallable, Category = "Editor|Main")
	void RefreshStats();

protected:
	// ============================================================
	// BindWidget: Sub-panels
	// ============================================================
	UPROPERTY(meta = (BindWidget))
	UEditorSidebarWidget* Sidebar;

	UPROPERTY(meta = (BindWidget))
	UEditorToolbarWidget* Toolbar;

	UPROPERTY(meta = (BindWidget))
	UGroupManagerPanel* GroupManager;

	UPROPERTY(meta = (BindWidget))
	UEditorStatusBar* StatusBar;

	// ============================================================
	// BindWidget: Dialog layer
	// ============================================================
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* DialogLayer;

	UPROPERTY(meta = (BindWidget))
	UButton* DialogOverlay;

	// ============================================================
	// Dialog TSubclassOf (set in Blueprint defaults)
	// ============================================================
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UConfirmDialog> ConfirmDialogClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UNewLevelDialog> NewLevelDialogClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UValidationResultPanel> ValidationPanelClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UColorPickerPopup> ColorPickerClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<ULoadLevelDialog> LoadLevelDialogClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<USaveLevelDialog> SaveLevelDialogClass;

	// ============================================================
	// Dialog state
	// ============================================================
	UPROPERTY()
	UUserWidget* CurrentDialog = nullptr;

	bool bIsDialogOpen = false;

	/** Pending confirm action stored for TFunction -> Dynamic Delegate bridging. */
	TFunction<void()> PendingConfirmAction;

	/** Pending validation context for force-confirm flow. */
	EValidationContext PendingValidationContext = EValidationContext::Save;

	/** Pending color picker group id. */
	int32 PendingColorPickerGroupId = 0;

	// ============================================================
	// GameMode reference
	// ============================================================
	UPROPERTY()
	ALevelEditorGameMode* GameMode = nullptr;

	// ============================================================
	// Lifecycle
	// ============================================================
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ============================================================
	// GameMode Delegate Handlers
	// ============================================================
	UFUNCTION()
	void HandleBrushChanged(EEditorBrush NewBrush);

	UFUNCTION()
	void HandleModeChanged(EEditorMode NewMode);

	UFUNCTION()
	void HandleGroupCreated(int32 GroupId);

	UFUNCTION()
	void HandleGroupDeleted(int32 GroupId);

	UFUNCTION()
	void HandleEditorError(const FText& Message);

	// ============================================================
	// Sub-panel Delegate Handlers
	// ============================================================
	UFUNCTION()
	void HandleSidebarBrushSelected(EEditorBrush Brush);

	UFUNCTION()
	void HandleSidebarVariantSelected(FName StyleId);

	UFUNCTION()
	void HandleToolbarNew();

	UFUNCTION()
	void HandleToolbarSave();

	UFUNCTION()
	void HandleToolbarLoad();

	UFUNCTION()
	void HandleToolbarTest();

	UFUNCTION()
	void HandleToolbarBack();

	UFUNCTION()
	void HandleGroupMgrSelectGroup(int32 GroupId);

	UFUNCTION()
	void HandleGroupMgrEditColor(int32 GroupId);

	UFUNCTION()
	void HandleGroupMgrDeleteGroup(int32 GroupId);

	UFUNCTION()
	void HandleGroupMgrDirectionCycle(int32 GroupId);

	// ============================================================
	// Dialog management
	// ============================================================
	void ShowDialog(UUserWidget* Dialog);
	void CloseDialog();

	UFUNCTION()
	void HandleOverlayClicked();

	// ============================================================
	// UFUNCTION relay methods for Dynamic Delegates
	// ============================================================

	/** ConfirmDialog confirm/cancel relay */
	UFUNCTION()
	void HandleConfirmDialogConfirmed();

	UFUNCTION()
	void HandleConfirmDialogCancelled();

	/** NewLevelDialog confirm relay (DYNAMIC delegate with params) */
	UFUNCTION()
	void HandleNewLevelConfirmed(int32 Width, int32 Height);

	/** ColorPickerPopup confirm relay (DYNAMIC delegate with params) */
	UFUNCTION()
	void HandleColorPickerConfirmed(int32 InGroupId, FLinearColor BaseColor, FLinearColor ActiveColor);

	/** SaveLevelDialog confirm relay */
	UFUNCTION()
	void HandleSaveLevelConfirmed(const FString& FileName);

	/** LoadLevelDialog confirm relay */
	UFUNCTION()
	void HandleLoadLevelConfirmed(const FString& FileName);

	/** ValidationResultPanel force-confirm / close relay */
	UFUNCTION()
	void HandleValidationForceConfirmed();

	UFUNCTION()
	void HandleValidationClosed();

	// ============================================================
	// Flow methods
	// ============================================================
	void ShowNewLevelDialog();
	void ShowSaveLevelDialog();
	void ShowLoadLevelDialog();
	void ShowColorPicker(int32 GroupId);
	void ShowValidationPanel(const FLevelValidationResult& Result, EValidationContext Context);
	void ShowConfirmDialog(const FText& Title, const FText& Message,
	                       const FText& ConfirmText, TFunction<void()> OnConfirm);
	void DoSave(const FString& FileName);
	void DoLoad(const FString& FileName);
};
