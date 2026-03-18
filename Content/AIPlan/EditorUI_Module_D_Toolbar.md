# Module D: 工具栏（Toolbar）

## 概述
顶部工具栏，包含 5 个操作按钮（新建/保存/加载/测试/返回）。按钮只负责广播 delegate，具体流程（弹窗、验证等）由 MainWidget 编排。

## 前置依赖
- 无编译依赖（可与 A/B/C/E 并行开发）
- 运行时依赖 Module A（ConfirmDialog, NewLevelDialog, ValidationResultPanel），由 MainWidget 串联

## 产出文件
```
Source/TuiXiangZi/UI/Editor/EditorToolbarWidget.h
Source/TuiXiangZi/UI/Editor/EditorToolbarWidget.cpp
Content/Blueprints/UI/Editor/WBP_EditorToolbar.uasset
```

---

## 类定义

```cpp
UCLASS()
class UEditorToolbarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // --- Delegates（供 MainWidget 绑定）---
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToolbarAction);

    UPROPERTY(BlueprintAssignable)
    FOnToolbarAction OnNewClicked;

    UPROPERTY(BlueprintAssignable)
    FOnToolbarAction OnSaveClicked;

    UPROPERTY(BlueprintAssignable)
    FOnToolbarAction OnLoadClicked;

    UPROPERTY(BlueprintAssignable)
    FOnToolbarAction OnTestClicked;

    UPROPERTY(BlueprintAssignable)
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

    virtual void NativeConstruct() override;

    UFUNCTION() void HandleNewClicked();
    UFUNCTION() void HandleSaveClicked();
    UFUNCTION() void HandleLoadClicked();
    UFUNCTION() void HandleTestClicked();
    UFUNCTION() void HandleBackClicked();
};
```

---

## 布局规格

```
┌──────────────────────────────────────────────────────────────┐
│  [📄 新建]  [💾 保存]  [📂 加载]  │  [▶ 测试]  │  [← 返回]  │
│  ← 左侧组: 文件操作              │  中间: 测试  │  右对齐     │
└──────────────────────────────────────────────────────────────┘
高度: 48px 固定
背景: #2A2A2A
左 Padding: 8px
```

**Slate 结构：**
```
HorizontalBox
├─ [NewButton]    图标+文字
├─ [Spacer 4px]
├─ [SaveButton]   图标+文字
├─ [Spacer 4px]
├─ [LoadButton]   图标+文字
├─ [Spacer 16px]  ← 分隔
├─ [Separator 1px 竖线]
├─ [Spacer 16px]
├─ [TestButton]   图标+文字
├─ [Spacer Fill]  ← 填充剩余空间
└─ [BackButton]   图标+文字, 右对齐
```

### 单个按钮布局
```
Button (Style: 无背景, Hover: #3A3A3A, Pressed: #1A1A1A)
└─ HorizontalBox
   ├─ [TextBlock 18px] 图标字符（或 Image 24x24）
   ├─ [Spacer 6px]
   └─ [TextBlock 14px] 按钮名称
Padding: 水平 12px, 垂直 8px
```

**按钮配置：**
| 按钮 | 图标 | 名称 | 快捷键提示 (Tooltip) |
|------|------|------|---------------------|
| New | 📄 | 新建 | Ctrl+N |
| Save | 💾 | 保存 | Ctrl+S |
| Load | 📂 | 加载 | Ctrl+O |
| Test | ▶ | 测试 | F5 |
| Back | ← | 返回 | Esc |

---

## 交互逻辑

### NativeConstruct()
```
NativeConstruct():
  ├─ NewButton->OnClicked.AddDynamic(this, &HandleNewClicked)
  ├─ SaveButton->OnClicked.AddDynamic(this, &HandleSaveClicked)
  ├─ LoadButton->OnClicked.AddDynamic(this, &HandleLoadClicked)
  ├─ TestButton->OnClicked.AddDynamic(this, &HandleTestClicked)
  └─ BackButton->OnClicked.AddDynamic(this, &HandleBackClicked)

  // Tooltip（显示快捷键）
  ├─ NewButton->SetToolTipText("新建关卡 (Ctrl+N)")
  ├─ SaveButton->SetToolTipText("保存关卡 (Ctrl+S)")
  ├─ LoadButton->SetToolTipText("加载关卡 (Ctrl+O)")
  ├─ TestButton->SetToolTipText("测试关卡 (F5)")
  └─ BackButton->SetToolTipText("返回主菜单 (Esc)")
```

### 按钮处理
```
HandleNewClicked():  → OnNewClicked.Broadcast()
HandleSaveClicked(): → OnSaveClicked.Broadcast()
HandleLoadClicked(): → OnLoadClicked.Broadcast()
HandleTestClicked(): → OnTestClicked.Broadcast()
HandleBackClicked(): → OnBackClicked.Broadcast()
```

Toolbar 本身不包含任何业务逻辑，只是一个纯 UI 按钮面板。所有流程编排在 MainWidget 中：

---

## MainWidget 中的流程编排（供 Module F 参考）

### OnNewClicked 处理
```
MainWidget.HandleToolbarNew():
  ├─ if GameMode->IsDirty():
  │   └─ ShowConfirmDialog("未保存的更改", "当前关卡有未保存的更改...", "继续"):
  │       ├─ OnConfirmed → ShowNewLevelDialog()
  │       └─ OnCancelled → 无操作
  └─ else:
      └─ ShowNewLevelDialog()

ShowNewLevelDialog():
  ├─ 创建/复用 NewLevelDialog 实例
  ├─ ShowDialog(NewLevelDialog)
  └─ NewLevelDialog.OnConfirmed(W, H):
      ├─ GameMode->NewLevel(W, H)
      ├─ CloseDialog()
      ├─ StatusBar->RefreshStats(0, 0, 0)
      └─ GroupMgr->ClearAll()
```

### OnSaveClicked 处理
```
MainWidget.HandleToolbarSave():
  ├─ FLevelValidationResult Result = GameMode->ValidateLevel()
  │
  ├─ if Result.HasErrors():
  │   └─ ShowValidationPanel(Result, EValidationContext::Save)
  │       └─ OnClosed → CloseDialog()  (无法保存)
  │
  ├─ elif Result.HasWarnings():
  │   └─ ShowValidationPanel(Result, EValidationContext::Save)
  │       ├─ OnForceConfirmed → CloseDialog() → DoSave()
  │       └─ OnClosed → CloseDialog()
  │
  └─ else:
      └─ DoSave()

DoSave():
  ├─ 确定文件名（简化方案: 使用固定名或自动递增）
  │   // 完整方案: 弹出文件名输入 Dialog
  ├─ bool bSuccess = GameMode->SaveLevel(FileName)
  ├─ if bSuccess:
  │   └─ StatusBar->ShowTemporaryMessage("保存成功: {FileName}")
  └─ else:
      └─ ShowConfirmDialog("保存失败", "无法保存文件...", "确定") // 仅提示
```

### OnLoadClicked 处理
```
MainWidget.HandleToolbarLoad():
  ├─ if GameMode->IsDirty():
  │   └─ ShowConfirmDialog(...):
  │       ├─ OnConfirmed → ShowLoadFileList()
  │       └─ OnCancelled → 无操作
  └─ else:
      └─ ShowLoadFileList()

ShowLoadFileList():
  ├─ 扫描 FPaths::ProjectSavedDir() / "CustomLevels/" 下的 .json 文件
  ├─ 显示文件列表弹窗（可复用 ConfirmDialog 或单独 Widget）
  └─ 用户选择文件:
      ├─ GameMode->LoadLevel(FileName)
      ├─ 成功:
      │   ├─ StatusBar->ShowTemporaryMessage("加载成功: {FileName}")
      │   ├─ GroupMgr->RebuildFromGameMode()
      │   └─ Sidebar->SetActiveBrush(EEditorBrush::Floor)
      └─ 失败 → 提示错误
```

### OnTestClicked 处理
```
MainWidget.HandleToolbarTest():
  ├─ FLevelValidationResult Result = GameMode->ValidateLevel()
  ├─ if Result.HasErrors():
  │   └─ ShowValidationPanel(Result, EValidationContext::Test) → 阻止
  └─ else:
      └─ GameMode->TestCurrentLevel()
          // 自动保存临时文件 + OpenLevel(GameMap)
```

### OnBackClicked 处理
```
MainWidget.HandleToolbarBack():
  ├─ if GameMode->IsDirty():
  │   └─ ShowConfirmDialog("返回主菜单", "未保存的更改将丢失...", "返回"):
  │       ├─ OnConfirmed → UGameplayStatics::OpenLevel("MainMenuMap")
  │       └─ OnCancelled → 无操作
  └─ else:
      └─ UGameplayStatics::OpenLevel("MainMenuMap")
```

---

## 验收标准
1. 5 个按钮正确显示图标、名称和 Tooltip
2. 点击每个按钮广播对应 delegate
3. 按钮有 Hover/Pressed 视觉反馈
4. 返回按钮右对齐，与其他按钮视觉分离
