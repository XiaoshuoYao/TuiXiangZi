#include "EditorMainWidget.h"
#include "ConfirmDialog.h"
#include "NewLevelDialog.h"
#include "ValidationResultPanel.h"
#include "EditorStatusBar.h"
#include "EditorSidebarWidget.h"
#include "EditorToolbarWidget.h"
#include "GroupManagerPanel.h"
#include "ColorPickerPopup.h"
#include "Editor/LevelEditorGameMode.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

// ============================================================
// Lifecycle
// ============================================================

void UEditorMainWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. Get GameMode
	GameMode = Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogTemp, Error, TEXT("UEditorMainWidget: Failed to get ALevelEditorGameMode!"));
		return;
	}

	// 2. Bind GameMode Delegates
	GameMode->OnBrushChanged.AddDynamic(this, &UEditorMainWidget::HandleBrushChanged);
	GameMode->OnEditorModeChanged.AddDynamic(this, &UEditorMainWidget::HandleModeChanged);
	GameMode->OnGroupCreated.AddDynamic(this, &UEditorMainWidget::HandleGroupCreated);
	GameMode->OnGroupDeleted.AddDynamic(this, &UEditorMainWidget::HandleGroupDeleted);

	// 3. Bind Sub-panel Delegates
	// Sidebar
	if (Sidebar)
	{
		Sidebar->OnBrushSelected.AddDynamic(this, &UEditorMainWidget::HandleSidebarBrushSelected);
		Sidebar->OnVariantSelected.AddDynamic(this, &UEditorMainWidget::HandleSidebarVariantSelected);
		Sidebar->InitializeWithCatalog(GameMode->GetTileStyleCatalog());
	}

	// Toolbar
	if (Toolbar)
	{
		Toolbar->OnNewClicked.AddDynamic(this, &UEditorMainWidget::HandleToolbarNew);
		Toolbar->OnSaveClicked.AddDynamic(this, &UEditorMainWidget::HandleToolbarSave);
		Toolbar->OnLoadClicked.AddDynamic(this, &UEditorMainWidget::HandleToolbarLoad);
		Toolbar->OnTestClicked.AddDynamic(this, &UEditorMainWidget::HandleToolbarTest);
		Toolbar->OnBackClicked.AddDynamic(this, &UEditorMainWidget::HandleToolbarBack);
	}

	// GroupManager
	if (GroupManager)
	{
		GroupManager->OnSelectGroup.AddDynamic(this, &UEditorMainWidget::HandleGroupMgrSelectGroup);
		GroupManager->OnEditGroupColor.AddDynamic(this, &UEditorMainWidget::HandleGroupMgrEditColor);
		GroupManager->OnDeleteGroup.AddDynamic(this, &UEditorMainWidget::HandleGroupMgrDeleteGroup);
		GroupManager->OnRequestNewGroup.AddDynamic(this, &UEditorMainWidget::HandleGroupMgrNewGroup);
	}

	// 4. Initialize dialog layer
	if (DialogLayer)
	{
		DialogLayer->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DialogOverlay)
	{
		DialogOverlay->OnClicked.AddDynamic(this, &UEditorMainWidget::HandleOverlayClicked);
	}

	// 5. Initial refresh
	HandleBrushChanged(GameMode->GetCurrentBrush());
	HandleModeChanged(GameMode->GetEditorMode());
	RefreshStats();
	if (GroupManager)
	{
		GroupManager->RebuildFromGameMode(GameMode);
	}
}

void UEditorMainWidget::NativeDestruct()
{
	if (GameMode)
	{
		GameMode->OnBrushChanged.RemoveDynamic(this, &UEditorMainWidget::HandleBrushChanged);
		GameMode->OnEditorModeChanged.RemoveDynamic(this, &UEditorMainWidget::HandleModeChanged);
		GameMode->OnGroupCreated.RemoveDynamic(this, &UEditorMainWidget::HandleGroupCreated);
		GameMode->OnGroupDeleted.RemoveDynamic(this, &UEditorMainWidget::HandleGroupDeleted);
	}

	Super::NativeDestruct();
}

// ============================================================
// GameMode Delegate Handlers
// ============================================================

void UEditorMainWidget::HandleBrushChanged(EEditorBrush NewBrush)
{
	if (Sidebar)
	{
		Sidebar->SetActiveBrush(NewBrush);
	}
	if (StatusBar)
	{
		StatusBar->RefreshBrushText(NewBrush);
	}
}

void UEditorMainWidget::HandleModeChanged(EEditorMode NewMode)
{
	if (StatusBar)
	{
		StatusBar->RefreshModeText(NewMode);
	}

	if (NewMode == EEditorMode::PlacingPlatesForDoor)
	{
		if (Sidebar)
		{
			Sidebar->SetEnabled(false);
		}
		if (GroupManager)
		{
			GroupManager->SetPlacementMode(true, GameMode->GetCurrentGroupId());
		}
	}
	else if (NewMode == EEditorMode::Normal)
	{
		if (Sidebar)
		{
			Sidebar->SetEnabled(true);
		}
		if (GroupManager)
		{
			GroupManager->SetPlacementMode(false);
		}
	}

	if (GroupManager)
	{
		GroupManager->RefreshActiveGroup(GameMode->GetCurrentGroupId());
	}
}

void UEditorMainWidget::HandleGroupCreated(int32 GroupId)
{
	if (GroupManager)
	{
		GroupManager->AddGroupEntry(GroupId, GameMode->GetGroupStyle(GroupId));
	}
	RefreshStats();
}

void UEditorMainWidget::HandleGroupDeleted(int32 GroupId)
{
	if (GroupManager)
	{
		GroupManager->RemoveGroupEntry(GroupId);
	}
	RefreshStats();
}

// ============================================================
// Sub-panel Delegate Handlers
// ============================================================

void UEditorMainWidget::HandleSidebarBrushSelected(EEditorBrush Brush)
{
	if (GameMode)
	{
		GameMode->SetCurrentBrush(Brush);
	}
}

void UEditorMainWidget::HandleSidebarVariantSelected(FName StyleId)
{
	if (GameMode)
	{
		GameMode->SetCurrentVisualStyleId(StyleId);
	}
}

void UEditorMainWidget::HandleToolbarNew()
{
	if (!GameMode) return;

	if (GameMode->IsDirty())
	{
		ShowConfirmDialog(
			NSLOCTEXT("Editor", "UnsavedTitle", "未保存的更改"),
			NSLOCTEXT("Editor", "UnsavedMsg", "当前关卡有未保存的更改，继续将丢失这些更改。"),
			NSLOCTEXT("Editor", "Continue", "继续"),
			[this]() { ShowNewLevelDialog(); });
	}
	else
	{
		ShowNewLevelDialog();
	}
}

void UEditorMainWidget::HandleToolbarSave()
{
	if (!GameMode) return;

	FLevelValidationResult Result = GameMode->ValidateLevel();
	if (Result.HasErrors())
	{
		ShowValidationPanel(Result, EValidationContext::Save);
	}
	else if (Result.HasWarnings())
	{
		ShowValidationPanel(Result, EValidationContext::Save);
	}
	else
	{
		DoSave();
	}
}

void UEditorMainWidget::HandleToolbarLoad()
{
	if (!GameMode) return;

	if (GameMode->IsDirty())
	{
		ShowConfirmDialog(
			NSLOCTEXT("Editor", "UnsavedTitle", "未保存的更改"),
			NSLOCTEXT("Editor", "UnsavedLoadMsg", "当前关卡有未保存的更改，继续将丢失这些更改。"),
			NSLOCTEXT("Editor", "Continue", "继续"),
			[this]()
			{
				// TODO: Show load file list / file browser
				// For now, load a fixed file name
				DoLoad(TEXT("DefaultLevel"));
			});
	}
	else
	{
		// TODO: Show load file list / file browser
		DoLoad(TEXT("DefaultLevel"));
	}
}

void UEditorMainWidget::HandleToolbarTest()
{
	if (!GameMode) return;

	FLevelValidationResult Result = GameMode->ValidateLevel();
	if (Result.HasErrors())
	{
		ShowValidationPanel(Result, EValidationContext::Test);
	}
	else
	{
		GameMode->TestCurrentLevel();
	}
}

void UEditorMainWidget::HandleToolbarBack()
{
	if (!GameMode) return;

	if (GameMode->IsDirty())
	{
		ShowConfirmDialog(
			NSLOCTEXT("Editor", "BackTitle", "返回主菜单"),
			NSLOCTEXT("Editor", "BackMsg", "未保存的更改将丢失，是否返回主菜单？"),
			NSLOCTEXT("Editor", "Back", "返回"),
			[this]() { UGameplayStatics::OpenLevel(this, TEXT("MainMenuMap")); });
	}
	else
	{
		UGameplayStatics::OpenLevel(this, TEXT("MainMenuMap"));
	}
}

void UEditorMainWidget::HandleGroupMgrSelectGroup(int32 GroupId)
{
	if (GameMode)
	{
		GameMode->SetCurrentGroupId(GroupId);
	}
	if (GroupManager)
	{
		GroupManager->RefreshActiveGroup(GroupId);
	}
}

void UEditorMainWidget::HandleGroupMgrEditColor(int32 GroupId)
{
	ShowColorPicker(GroupId);
}

void UEditorMainWidget::HandleGroupMgrDeleteGroup(int32 GroupId)
{
	if (!GameMode) return;

	FMechanismGroupStyleData Style = GameMode->GetGroupStyle(GroupId);
	ShowConfirmDialog(
		NSLOCTEXT("Editor", "DeleteGroup", "删除分组"),
		FText::Format(NSLOCTEXT("Editor", "DeleteGroupMsg",
			"删除分组 \"{0}\" 将同时移除所有关联的门和压力板，是否继续？"),
			FText::FromString(Style.DisplayName)),
		NSLOCTEXT("Editor", "Delete", "删除"),
		[this, GroupId]() { GameMode->DeleteGroup(GroupId); });
}

void UEditorMainWidget::HandleGroupMgrNewGroup()
{
	if (GameMode)
	{
		GameMode->CreateNewGroup();
		// OnGroupCreated delegate -> HandleGroupCreated will add the entry automatically
	}
}

// ============================================================
// Dialog Management
// ============================================================

void UEditorMainWidget::ShowDialog(UUserWidget* Dialog)
{
	if (!Dialog || !DialogLayer) return;

	if (bIsDialogOpen)
	{
		CloseDialog();
	}

	CurrentDialog = Dialog;
	bIsDialogOpen = true;

	UCanvasPanelSlot* DialogSlot = DialogLayer->AddChildToCanvas(Dialog);
	if (DialogSlot)
	{
		DialogSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		DialogSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		DialogSlot->SetAutoSize(true);
	}

	DialogLayer->SetVisibility(ESlateVisibility::Visible);
}

void UEditorMainWidget::CloseDialog()
{
	if (!bIsDialogOpen) return;

	if (CurrentDialog)
	{
		CurrentDialog->RemoveFromParent();
		CurrentDialog = nullptr;
	}

	bIsDialogOpen = false;
	PendingConfirmAction = nullptr;

	if (DialogLayer)
	{
		DialogLayer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UEditorMainWidget::HandleOverlayClicked()
{
	CloseDialog();
}

// ============================================================
// UFUNCTION relay for Dynamic Delegates
// ============================================================

void UEditorMainWidget::HandleConfirmDialogConfirmed()
{
	CloseDialog();
	if (PendingConfirmAction)
	{
		TFunction<void()> Action = MoveTemp(PendingConfirmAction);
		PendingConfirmAction = nullptr;
		Action();
	}
}

void UEditorMainWidget::HandleConfirmDialogCancelled()
{
	CloseDialog();
	PendingConfirmAction = nullptr;
}

void UEditorMainWidget::HandleNewLevelConfirmed(int32 Width, int32 Height)
{
	CloseDialog();
	if (GameMode)
	{
		GameMode->NewLevel(Width, Height);
		RefreshStats();
		if (GroupManager)
		{
			GroupManager->ClearAll();
		}
	}
}

void UEditorMainWidget::HandleColorPickerConfirmed(int32 InGroupId, FLinearColor BaseColor, FLinearColor ActiveColor)
{
	if (GameMode)
	{
		GameMode->SetGroupColor(InGroupId, BaseColor, ActiveColor);
	}
	if (GroupManager)
	{
		GroupManager->UpdateGroupColor(InGroupId, BaseColor);
	}
	CloseDialog();
}

void UEditorMainWidget::HandleValidationForceConfirmed()
{
	CloseDialog();
	DoSave();
}

void UEditorMainWidget::HandleValidationClosed()
{
	CloseDialog();
}

// ============================================================
// Esc Key Handling
// ============================================================

bool UEditorMainWidget::HandleEscPressed()
{
	// Priority 1: Close dialog
	if (bIsDialogOpen)
	{
		CloseDialog();
		return true;
	}

	// Priority 2: Cancel placement mode
	if (GameMode && GameMode->GetEditorMode() == EEditorMode::PlacingPlatesForDoor)
	{
		GameMode->CancelPlacementMode();
		return true;
	}

	// Priority 3: Back to main menu
	HandleToolbarBack();
	return true;
}

// ============================================================
// Erase Confirm
// ============================================================

void UEditorMainWidget::RequestEraseConfirm(FIntPoint GridPos)
{
	if (!GameMode) return;

	FString Warning = GameMode->GetEraseWarning(GridPos);
	ShowConfirmDialog(
		NSLOCTEXT("Editor", "ConfirmErase", "确认擦除"),
		FText::FromString(Warning),
		NSLOCTEXT("Editor", "Erase", "擦除"),
		[this, GridPos]()
		{
			GameMode->EraseAtGrid(GridPos);
			RefreshStats();
		});
}

// ============================================================
// Public request methods
// ============================================================

void UEditorMainWidget::RequestSetBrush(EEditorBrush Brush)
{
	if (GameMode)
	{
		GameMode->SetCurrentBrush(Brush);
	}
}

void UEditorMainWidget::RequestToolbarAction(EToolbarAction Action)
{
	switch (Action)
	{
	case EToolbarAction::New:  HandleToolbarNew();  break;
	case EToolbarAction::Save: HandleToolbarSave(); break;
	case EToolbarAction::Load: HandleToolbarLoad(); break;
	case EToolbarAction::Test: HandleToolbarTest(); break;
	case EToolbarAction::Back: HandleToolbarBack(); break;
	}
}

void UEditorMainWidget::RefreshStats()
{
	if (StatusBar && GameMode)
	{
		StatusBar->RefreshStats(
			GameMode->GetCellCount(),
			GameMode->GetBoxCount(),
			GameMode->GetGroupCount());
	}
}

// ============================================================
// Flow Methods
// ============================================================

void UEditorMainWidget::ShowConfirmDialog(const FText& Title, const FText& Message,
                                          const FText& ConfirmText, TFunction<void()> OnConfirm)
{
	if (!ConfirmDialogClass) return;

	UConfirmDialog* Dialog = CreateWidget<UConfirmDialog>(this, ConfirmDialogClass);
	if (!Dialog) return;

	Dialog->Setup(Title, Message, ConfirmText);
	Dialog->OnConfirmed.AddDynamic(this, &UEditorMainWidget::HandleConfirmDialogConfirmed);
	Dialog->OnCancelled.AddDynamic(this, &UEditorMainWidget::HandleConfirmDialogCancelled);

	PendingConfirmAction = MoveTemp(OnConfirm);
	ShowDialog(Dialog);
}

void UEditorMainWidget::ShowNewLevelDialog()
{
	if (!NewLevelDialogClass) return;

	UNewLevelDialog* Dialog = CreateWidget<UNewLevelDialog>(this, NewLevelDialogClass);
	if (!Dialog) return;

	Dialog->OnConfirmed.AddDynamic(this, &UEditorMainWidget::HandleNewLevelConfirmed);
	Dialog->OnCancelled.AddDynamic(this, &UEditorMainWidget::HandleConfirmDialogCancelled);

	ShowDialog(Dialog);
}

void UEditorMainWidget::ShowColorPicker(int32 GroupId)
{
	if (!ColorPickerClass || !GameMode) return;

	UColorPickerPopup* Picker = CreateWidget<UColorPickerPopup>(this, ColorPickerClass);
	if (!Picker) return;

	FMechanismGroupStyleData Style = GameMode->GetGroupStyle(GroupId);
	Picker->Setup(GroupId, Style.BaseColor);
	Picker->OnColorConfirmed.AddDynamic(this, &UEditorMainWidget::HandleColorPickerConfirmed);
	Picker->OnCancelled.AddDynamic(this, &UEditorMainWidget::HandleConfirmDialogCancelled);

	ShowDialog(Picker);
}

void UEditorMainWidget::ShowValidationPanel(const FLevelValidationResult& Result, EValidationContext Context)
{
	if (!ValidationPanelClass) return;

	UValidationResultPanel* Panel = CreateWidget<UValidationResultPanel>(this, ValidationPanelClass);
	if (!Panel) return;

	Panel->Setup(Result, Context);
	Panel->OnForceConfirmed.AddDynamic(this, &UEditorMainWidget::HandleValidationForceConfirmed);
	Panel->OnClosed.AddDynamic(this, &UEditorMainWidget::HandleValidationClosed);

	PendingValidationContext = Context;
	ShowDialog(Panel);
}

void UEditorMainWidget::DoSave()
{
	if (!GameMode) return;

	// Simplified: use a fixed file name or auto-increment
	// TODO: Implement proper file naming / file browser
	FString FileName = TEXT("EditorLevel_Save");
	bool bSuccess = GameMode->SaveLevel(FileName);

	if (StatusBar)
	{
		if (bSuccess)
		{
			StatusBar->ShowTemporaryMessage(
				FText::Format(NSLOCTEXT("Editor", "SaveSuccess", "保存成功: {0}"),
					FText::FromString(FileName)));
		}
		else
		{
			StatusBar->ShowTemporaryMessage(
				NSLOCTEXT("Editor", "SaveFailed", "保存失败!"));
		}
	}
}

void UEditorMainWidget::DoLoad(const FString& FileName)
{
	if (!GameMode) return;

	bool bSuccess = GameMode->LoadLevel(FileName);

	if (bSuccess)
	{
		RefreshStats();
		if (GroupManager)
		{
			GroupManager->RebuildFromGameMode(GameMode);
		}
		HandleBrushChanged(GameMode->GetCurrentBrush());
		HandleModeChanged(GameMode->GetEditorMode());

		if (StatusBar)
		{
			StatusBar->ShowTemporaryMessage(
				FText::Format(NSLOCTEXT("Editor", "LoadSuccess", "加载成功: {0}"),
					FText::FromString(FileName)));
		}
	}
	else
	{
		if (StatusBar)
		{
			StatusBar->ShowTemporaryMessage(
				NSLOCTEXT("Editor", "LoadFailed", "加载失败!"));
		}
	}
}
