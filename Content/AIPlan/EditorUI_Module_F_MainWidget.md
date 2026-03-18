# Module F: 根容器与集成（EditorMainWidget）

## 概述
EditorMainWidget 是编辑器 UI 的根容器和事件路由中心。它持有所有子面板引用，绑定 GameMode delegate，管理弹窗层，编排所有交互流程。

## 前置依赖
- **编译依赖:** Module A/B/C/D/E 的所有头文件（持有子面板指针）
- **必须在 A~E 全部完成后开发**

## 产出文件
```
Source/TuiXiangZi/UI/Editor/EditorMainWidget.h
Source/TuiXiangZi/UI/Editor/EditorMainWidget.cpp
Content/Blueprints/UI/Editor/WBP_EditorMainWidget.uasset
```

---

## 类定义

```cpp
UCLASS()
class UEditorMainWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 供 Pawn 查询弹窗状态（屏蔽 Viewport 输入）
    UFUNCTION(BlueprintCallable)
    bool IsDialogOpen() const { return bIsDialogOpen; }

    // 供 Pawn 的 Esc 键调用
    UFUNCTION(BlueprintCallable)
    bool HandleEscPressed();

    // 供 Pawn 的擦除确认调用
    UFUNCTION(BlueprintCallable)
    void RequestEraseConfirm(FIntPoint GridPos);

    // 供 Pawn 的快捷键调用
    UFUNCTION(BlueprintCallable)
    void RequestSetBrush(EEditorBrush Brush);

    UFUNCTION(BlueprintCallable)
    void RequestToolbarAction(EToolbarAction Action); // New/Save/Load/Test/Back

    // 供外部刷新统计（每次 Paint/Erase 后）
    UFUNCTION(BlueprintCallable)
    void RefreshStats();

protected:
    // --- BindWidget: 子面板 ---
    UPROPERTY(meta = (BindWidget))
    UEditorSidebarWidget* Sidebar;

    UPROPERTY(meta = (BindWidget))
    UEditorToolbarWidget* Toolbar;

    UPROPERTY(meta = (BindWidget))
    UGroupManagerPanel* GroupManager;

    UPROPERTY(meta = (BindWidget))
    UEditorStatusBar* StatusBar;

    // --- BindWidget: 弹窗层 ---
    UPROPERTY(meta = (BindWidget))
    UCanvasPanel* DialogLayer;         // ZOrder 高于所有面板

    UPROPERTY(meta = (BindWidget))
    UButton* DialogOverlay;            // 全屏半透明遮罩（点击关闭弹窗）

    // --- 弹窗实例（按需创建/复用）---
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UConfirmDialog> ConfirmDialogClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UNewLevelDialog> NewLevelDialogClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UValidationResultPanel> ValidationPanelClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UColorPickerPopup> ColorPickerClass;

    UPROPERTY()
    UUserWidget* CurrentDialog = nullptr;

    bool bIsDialogOpen = false;

    // --- GameMode 引用 ---
    UPROPERTY()
    ALevelEditorGameMode* GameMode = nullptr;

    // --- 生命周期 ---
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // --- GameMode Delegate 处理 ---
    UFUNCTION() void HandleBrushChanged(EEditorBrush NewBrush);
    UFUNCTION() void HandleModeChanged(EEditorMode NewMode);
    UFUNCTION() void HandleGroupCreated(int32 GroupId);
    UFUNCTION() void HandleGroupDeleted(int32 GroupId);

    // --- 子面板 Delegate 处理 ---
    UFUNCTION() void HandleSidebarBrushSelected(EEditorBrush Brush);
    UFUNCTION() void HandleSidebarVariantSelected(FName StyleId);
    UFUNCTION() void HandleToolbarNew();
    UFUNCTION() void HandleToolbarSave();
    UFUNCTION() void HandleToolbarLoad();
    UFUNCTION() void HandleToolbarTest();
    UFUNCTION() void HandleToolbarBack();
    UFUNCTION() void HandleGroupMgrSelectGroup(int32 GroupId);
    UFUNCTION() void HandleGroupMgrEditColor(int32 GroupId);
    UFUNCTION() void HandleGroupMgrDeleteGroup(int32 GroupId);
    UFUNCTION() void HandleGroupMgrNewGroup();

    // --- 弹窗管理 ---
    void ShowDialog(UUserWidget* Dialog);
    void CloseDialog();
    UFUNCTION() void HandleOverlayClicked();

    // --- 流程方法 ---
    void ShowNewLevelDialog();
    void ShowColorPicker(int32 GroupId);
    void ShowValidationPanel(const FLevelValidationResult& Result, EValidationContext Context);
    void ShowConfirmDialog(const FText& Title, const FText& Message,
                           const FText& ConfirmText, TFunction<void()> OnConfirm);
    void DoSave();
    void DoLoad(const FString& FileName);
};
```

---

## 布局规格

```
CanvasPanel (根, 全屏)
│
├─ [Anchors: Top, HAlign=Fill, 48px]
│   └─ Toolbar
│
├─ [Anchors: Left, Top+48 to Bottom-32, W=200px]
│   └─ Sidebar
│
├─ [Anchors: Right, Top+48 to Bottom-32, W=250px]
│   └─ GroupManager
│
├─ [Anchors: Bottom, HAlign=Fill, 32px]
│   └─ StatusBar
│
├─ [中央区域: 无 Widget, 点击穿透到 Viewport]
│
└─ DialogLayer [Anchors: Fill, ZOrder=100]
    ├─ DialogOverlay [Anchors: Fill, 半透明黑色]
    │   └─ Button (全屏, Style=透明, OnClicked=HandleOverlayClicked)
    └─ [弹窗 Widget 居中放置]
```

**Visibility 规则：**
- Sidebar, Toolbar, StatusBar, GroupManager: `ESlateVisibility::Visible`（消耗鼠标）
- 中央 Viewport 区域: 不放 Widget（自动穿透到 Pawn）
- DialogLayer: 默认 `Collapsed`, 显示弹窗时设为 `Visible`
- DialogOverlay: 背景色 `#000000` Alpha `0.5`

---

## NativeConstruct（初始化全流程）

```
NativeConstruct():
  │
  ├─ 1. 获取 GameMode
  │   GameMode = Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode())
  │   if (!GameMode) { UE_LOG(Error, "..."); return; }
  │
  ├─ 2. 绑定 GameMode Delegates
  │   GameMode->OnBrushChanged.AddDynamic(this, &HandleBrushChanged)
  │   GameMode->OnEditorModeChanged.AddDynamic(this, &HandleModeChanged)
  │   GameMode->OnGroupCreated.AddDynamic(this, &HandleGroupCreated)
  │   GameMode->OnGroupDeleted.AddDynamic(this, &HandleGroupDeleted)
  │
  ├─ 3. 绑定子面板 Delegates
  │   // Sidebar
  │   Sidebar->OnBrushSelected.AddDynamic(this, &HandleSidebarBrushSelected)
  │   Sidebar->OnVariantSelected.AddDynamic(this, &HandleSidebarVariantSelected)
  │   Sidebar->InitializeWithCatalog(GameMode->GetTileStyleCatalog())
  │   // Toolbar
  │   Toolbar->OnNewClicked.AddDynamic(this, &HandleToolbarNew)
  │   Toolbar->OnSaveClicked.AddDynamic(this, &HandleToolbarSave)
  │   Toolbar->OnLoadClicked.AddDynamic(this, &HandleToolbarLoad)
  │   Toolbar->OnTestClicked.AddDynamic(this, &HandleToolbarTest)
  │   Toolbar->OnBackClicked.AddDynamic(this, &HandleToolbarBack)
  │   // GroupManager
  │   GroupManager->OnSelectGroup.AddDynamic(this, &HandleGroupMgrSelectGroup)
  │   GroupManager->OnEditGroupColor.AddDynamic(this, &HandleGroupMgrEditColor)
  │   GroupManager->OnDeleteGroup.AddDynamic(this, &HandleGroupMgrDeleteGroup)
  │   GroupManager->OnRequestNewGroup.AddDynamic(this, &HandleGroupMgrNewGroup)
  │
  ├─ 4. 初始化弹窗层
  │   DialogLayer->SetVisibility(ESlateVisibility::Collapsed)
  │   DialogOverlay->OnClicked.AddDynamic(this, &HandleOverlayClicked)
  │
  └─ 5. 初始刷新
      HandleBrushChanged(GameMode->GetCurrentBrush())
      HandleModeChanged(GameMode->GetEditorMode())
      RefreshStats()
      GroupManager->RebuildFromGameMode(GameMode)
```

---

## GameMode Delegate 处理

```cpp
void HandleBrushChanged(EEditorBrush NewBrush)
{
    Sidebar->SetActiveBrush(NewBrush);
    StatusBar->RefreshBrushText(NewBrush);
}

void HandleModeChanged(EEditorMode NewMode)
{
    StatusBar->RefreshModeText(NewMode);

    if (NewMode == EEditorMode::PlacingPlatesForDoor)
    {
        Sidebar->SetEnabled(false);
        GroupManager->SetPlacementMode(true, GameMode->GetCurrentGroupId());
    }
    else if (NewMode == EEditorMode::Normal)
    {
        Sidebar->SetEnabled(true);
        GroupManager->SetPlacementMode(false);
    }

    GroupManager->RefreshActiveGroup(GameMode->GetCurrentGroupId());
}

void HandleGroupCreated(int32 GroupId)
{
    GroupManager->AddGroupEntry(GroupId, GameMode->GetGroupStyle(GroupId));
    RefreshStats();
}

void HandleGroupDeleted(int32 GroupId)
{
    GroupManager->RemoveGroupEntry(GroupId);
    RefreshStats();
}
```

---

## 子面板 Delegate 处理

```cpp
void HandleSidebarBrushSelected(EEditorBrush Brush)
{
    GameMode->SetCurrentBrush(Brush);
    // GameMode 广播 OnBrushChanged → HandleBrushChanged 更新 UI
}

void HandleSidebarVariantSelected(FName StyleId)
{
    GameMode->SetCurrentVisualStyleId(StyleId);
}

void HandleToolbarNew()
{
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

void HandleToolbarSave()
{
    FLevelValidationResult Result = GameMode->ValidateLevel();
    if (Result.HasErrors())
    {
        ShowValidationPanel(Result, EValidationContext::Save);
    }
    else if (Result.HasWarnings())
    {
        ShowValidationPanel(Result, EValidationContext::Save);
        // ValidationPanel 的 OnForceConfirmed → DoSave()
    }
    else
    {
        DoSave();
    }
}

void HandleToolbarLoad()
{
    if (GameMode->IsDirty())
    {
        ShowConfirmDialog(..., [this]() { /* ShowLoadFileList */ });
    }
    else
    {
        // ShowLoadFileList
    }
}

void HandleToolbarTest()
{
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

void HandleToolbarBack()
{
    if (GameMode->IsDirty())
    {
        ShowConfirmDialog(
            NSLOCTEXT("Editor", "BackTitle", "返回主菜单"),
            NSLOCTEXT("Editor", "BackMsg", "未保存的更改将丢失，是否返回主菜单？"),
            NSLOCTEXT("Editor", "Back", "返回"),
            [this]() { UGameplayStatics::OpenLevel(this, "MainMenuMap"); });
    }
    else
    {
        UGameplayStatics::OpenLevel(this, "MainMenuMap");
    }
}

void HandleGroupMgrSelectGroup(int32 GroupId)
{
    GameMode->SetCurrentGroupId(GroupId);
    GroupManager->RefreshActiveGroup(GroupId);
}

void HandleGroupMgrEditColor(int32 GroupId)
{
    ShowColorPicker(GroupId);
}

void HandleGroupMgrDeleteGroup(int32 GroupId)
{
    FMechanismGroupStyleData Style = GameMode->GetGroupStyle(GroupId);
    ShowConfirmDialog(
        NSLOCTEXT("Editor", "DeleteGroup", "删除分组"),
        FText::Format(NSLOCTEXT("Editor", "DeleteGroupMsg",
            "删除分组 \"{0}\" 将同时移除所有关联的门和压力板，是否继续？"),
            FText::FromString(Style.DisplayName)),
        NSLOCTEXT("Editor", "Delete", "删除"),
        [this, GroupId]() { GameMode->DeleteGroup(GroupId); });
}

void HandleGroupMgrNewGroup()
{
    GameMode->CreateNewGroup();
    // OnGroupCreated delegate → HandleGroupCreated 自动添加条目
}
```

---

## 弹窗管理

```cpp
void ShowDialog(UUserWidget* Dialog)
{
    if (bIsDialogOpen) CloseDialog();  // 关闭已有弹窗

    CurrentDialog = Dialog;
    bIsDialogOpen = true;

    // 将弹窗添加到 DialogLayer 居中
    UCanvasPanelSlot* Slot = DialogLayer->AddChildToCanvas(Dialog);
    Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
    Slot->SetAlignment(FVector2D(0.5f, 0.5f));
    Slot->SetAutoSize(true);

    DialogLayer->SetVisibility(ESlateVisibility::Visible);
}

void CloseDialog()
{
    if (!bIsDialogOpen) return;

    if (CurrentDialog)
    {
        CurrentDialog->RemoveFromParent();
        CurrentDialog = nullptr;
    }

    bIsDialogOpen = false;
    DialogLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void HandleOverlayClicked()
{
    // 遮罩点击 → 等同于取消
    CloseDialog();
}
```

---

## Esc 键处理

```cpp
bool HandleEscPressed()
{
    // 优先级 1: 关闭弹窗
    if (bIsDialogOpen)
    {
        CloseDialog();
        return true;
    }

    // 优先级 2: 取消放置模式
    if (GameMode->GetEditorMode() == EEditorMode::PlacingPlatesForDoor)
    {
        GameMode->CancelPlacementMode();
        return true;
    }

    // 优先级 3: 返回主菜单
    HandleToolbarBack();
    return true;
}
```

---

## 擦除确认

```cpp
void RequestEraseConfirm(FIntPoint GridPos)
{
    FString Warning = GameMode->GetEraseWarning(GridPos);
    ShowConfirmDialog(
        NSLOCTEXT("Editor", "ConfirmErase", "确认擦除"),
        FText::FromString(Warning),
        NSLOCTEXT("Editor", "Erase", "擦除"),
        [this, GridPos]() { GameMode->EraseAtGrid(GridPos); RefreshStats(); });
}
```

---

## 辅助方法

```cpp
void ShowConfirmDialog(const FText& Title, const FText& Message,
                       const FText& ConfirmText, TFunction<void()> OnConfirm)
{
    UConfirmDialog* Dialog = CreateWidget<UConfirmDialog>(this, ConfirmDialogClass);
    Dialog->Setup(Title, Message, ConfirmText);

    // 注意: TFunction 不能直接绑定到 Dynamic Delegate
    // 方案: 使用成员变量缓存 lambda，或改用非 Dynamic delegate
    // 简化方案: 用 FOnConfirmResult + 标记变量判断上下文

    Dialog->OnConfirmed.AddDynamic(this, &HandleConfirmDialogConfirmed);
    Dialog->OnCancelled.AddDynamic(this, &HandleConfirmDialogCancelled);

    PendingConfirmAction = OnConfirm;  // TFunction<void()> 成员变量
    ShowDialog(Dialog);
}

void HandleConfirmDialogConfirmed()
{
    CloseDialog();
    if (PendingConfirmAction) { PendingConfirmAction(); PendingConfirmAction = nullptr; }
}

void HandleConfirmDialogCancelled()
{
    CloseDialog();
    PendingConfirmAction = nullptr;
}

void RefreshStats()
{
    StatusBar->RefreshStats(
        GameMode->GetCellCount(),
        GameMode->GetBoxCount(),
        GameMode->GetGroupCount());
}

void ShowNewLevelDialog()
{
    UNewLevelDialog* Dialog = CreateWidget<UNewLevelDialog>(this, NewLevelDialogClass);
    Dialog->OnConfirmed.AddLambda([this](int32 W, int32 H) {
        CloseDialog();
        GameMode->NewLevel(W, H);
        RefreshStats();
        GroupManager->ClearAll();
    });
    Dialog->OnCancelled.AddDynamic(this, &HandleConfirmDialogCancelled); // 复用
    ShowDialog(Dialog);
}

void ShowColorPicker(int32 GroupId)
{
    UColorPickerPopup* Picker = CreateWidget<UColorPickerPopup>(this, ColorPickerClass);
    FMechanismGroupStyleData Style = GameMode->GetGroupStyle(GroupId);
    Picker->Setup(GroupId, Style.BaseColor);
    Picker->OnColorConfirmed.AddLambda([this](int32 Id, FLinearColor Base, FLinearColor Active) {
        GameMode->SetGroupColor(Id, Base, Active);
        GroupManager->UpdateGroupColor(Id, Base);
        CloseDialog();
    });
    Picker->OnCancelled.AddDynamic(this, &HandleConfirmDialogCancelled);
    ShowDialog(Picker);
}

void ShowValidationPanel(const FLevelValidationResult& Result, EValidationContext Context)
{
    UValidationResultPanel* Panel = CreateWidget<UValidationResultPanel>(this, ValidationPanelClass);
    Panel->Setup(Result, Context);
    Panel->OnForceConfirmed.AddLambda([this]() {
        CloseDialog();
        DoSave();
    });
    Panel->OnClosed.AddLambda([this]() { CloseDialog(); });
    ShowDialog(Panel);
}
```

---

## 验收标准
1. NativeConstruct 中所有 delegate 绑定成功，无崩溃
2. GameMode delegate 回调正确路由到子面板的 Refresh 方法
3. 弹窗显示/关闭/遮罩点击/Esc 关闭全部正常
4. 同时只有一个弹窗打开
5. 弹窗打开时 `IsDialogOpen()` 返回 true
6. Esc 三级优先级正确
7. 完整流程测试: 新建/保存（含验证）/加载/测试/返回，每条路径的弹窗链正确
8. 擦除确认流程: 弹窗→确认→执行 / 弹窗→取消→无操作
9. 分组操作: 创建/删除（含确认）/选中/改颜色 全链路通畅
