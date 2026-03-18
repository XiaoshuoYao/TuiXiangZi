# Module A: 弹窗系统（Dialogs）

## 概述
实现 3 个弹窗 Widget：通用确认弹窗、新建关卡弹窗、验证结果面板。它们共享弹窗模式（居中显示、遮罩背景、Esc 关闭），可作为一组开发。

## 前置依赖
- 无（批次 1 的一部分，最先开发）

## 产出文件
```
Source/TuiXiangZi/UI/Editor/ConfirmDialog.h
Source/TuiXiangZi/UI/Editor/ConfirmDialog.cpp
Source/TuiXiangZi/UI/Editor/NewLevelDialog.h
Source/TuiXiangZi/UI/Editor/NewLevelDialog.cpp
Source/TuiXiangZi/UI/Editor/ValidationResultPanel.h
Source/TuiXiangZi/UI/Editor/ValidationResultPanel.cpp
Content/Blueprints/UI/Editor/WBP_ConfirmDialog.uasset
Content/Blueprints/UI/Editor/WBP_NewLevelDialog.uasset
Content/Blueprints/UI/Editor/WBP_ValidationResultPanel.uasset
```

---

## Widget 1: UConfirmDialog（通用确认弹窗）

### 类定义
```cpp
UCLASS()
class UConfirmDialog : public UUserWidget
{
    GENERATED_BODY()
public:
    // --- 公共接口 ---
    UFUNCTION(BlueprintCallable)
    void Setup(const FText& Title, const FText& Message,
               const FText& ConfirmText = NSLOCTEXT("Editor", "OK", "确定"),
               const FText& CancelText = NSLOCTEXT("Editor", "Cancel", "取消"));

    // --- Delegates（供 MainWidget 绑定）---
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogResult);

    UPROPERTY(BlueprintAssignable)
    FOnDialogResult OnConfirmed;

    UPROPERTY(BlueprintAssignable)
    FOnDialogResult OnCancelled;

protected:
    // --- BindWidget（在 Blueprint 中绑定）---
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* MessageText;

    UPROPERTY(meta = (BindWidget))
    UButton* ConfirmButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CancelButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CloseButton; // 右上角 X

    // --- 内部方法 ---
    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleConfirmClicked();

    UFUNCTION()
    void HandleCancelClicked();
};
```

### 布局规格
```
┌──────────────────────────┐
│  {Title}           [X]   │   ← 标题栏 36px，深灰背景
├──────────────────────────┤
│                          │
│  {Message}               │   ← 内容区，自动换行，最大宽度 400px
│                          │
│    [取消]    [确定]      │   ← 按钮区 48px，右对齐
└──────────────────────────┘
整体: 最小宽度 300px, 最大宽度 450px, 居中于屏幕
背景: SizeBox → Border(圆角 8px, 深色背景 #2A2A2A)
```

### 交互逻辑
```
NativeConstruct():
  ├─ ConfirmButton->OnClicked.AddDynamic(this, &HandleConfirmClicked)
  ├─ CancelButton->OnClicked.AddDynamic(this, &HandleCancelClicked)
  └─ CloseButton->OnClicked.AddDynamic(this, &HandleCancelClicked)

HandleConfirmClicked():
  └─ OnConfirmed.Broadcast()
      // 注意: 不在这里关闭自己，由 MainWidget 的弹窗管理统一关闭

HandleCancelClicked():
  └─ OnCancelled.Broadcast()

Setup() 调用时:
  ├─ TitleText->SetText(Title)
  ├─ MessageText->SetText(Message)
  ├─ ConfirmButton 的 TextBlock->SetText(ConfirmText)
  └─ CancelButton 的 TextBlock->SetText(CancelText)
```

### 使用场景
| 调用方 | Title | Message | ConfirmText |
|--------|-------|---------|-------------|
| Toolbar.新建 | "未保存的更改" | "当前关卡有未保存的更改，继续将丢失这些更改。" | "继续" |
| Toolbar.加载 | "未保存的更改" | "当前关卡有未保存的更改，继续将丢失这些更改。" | "继续" |
| Toolbar.返回 | "返回主菜单" | "未保存的更改将丢失，是否返回主菜单？" | "返回" |
| GroupMgr.删除 | "删除分组" | "删除分组 '{Name}' 将同时移除所有关联的门和压力板，是否继续？" | "删除" |
| Pawn.擦除确认 | "确认擦除" | "{GetEraseWarning(Pos) 返回的文本}" | "擦除" |

---

## Widget 2: UNewLevelDialog（新建关卡弹窗）

### 类定义
```cpp
UCLASS()
class UNewLevelDialog : public UUserWidget
{
    GENERATED_BODY()
public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNewLevelConfirmed, int32, Width, int32, Height);

    UPROPERTY(BlueprintAssignable)
    FOnNewLevelConfirmed OnConfirmed;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNewLevelCancelled);

    UPROPERTY(BlueprintAssignable)
    FOnNewLevelCancelled OnCancelled;

protected:
    UPROPERTY(meta = (BindWidget))
    USpinBox* WidthSpinBox;

    UPROPERTY(meta = (BindWidget))
    USpinBox* HeightSpinBox;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* PreviewText;   // "8 × 8 = 64 格"

    UPROPERTY(meta = (BindWidget))
    UButton* ConfirmButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CancelButton;

    UPROPERTY(meta = (BindWidget))
    UButton* CloseButton;

    virtual void NativeConstruct() override;

    UFUNCTION()
    void HandleWidthChanged(float Value);

    UFUNCTION()
    void HandleHeightChanged(float Value);

    UFUNCTION()
    void HandleConfirmClicked();

    UFUNCTION()
    void HandleCancelClicked();

    void UpdatePreview();
};
```

### 布局规格
```
┌──────────────────────────┐
│  新建关卡          [X]   │
├──────────────────────────┤
│                          │
│  宽度:  [  8  ] [▲][▼]  │   ← SpinBox, 水平排列
│  高度:  [  8  ] [▲][▼]  │
│                          │
│  预览: 8 × 8 = 64 格    │   ← 灰色提示文本
│                          │
│    [取消]    [确定]      │
└──────────────────────────┘
整体: 固定宽度 320px, 居中
```

### 交互逻辑
```
NativeConstruct():
  ├─ WidthSpinBox: MinValue=3, MaxValue=30, Value=8, Delta=1
  ├─ HeightSpinBox: MinValue=3, MaxValue=30, Value=8, Delta=1
  ├─ WidthSpinBox->OnValueChanged.AddDynamic(HandleWidthChanged)
  ├─ HeightSpinBox->OnValueChanged.AddDynamic(HandleHeightChanged)
  ├─ ConfirmButton->OnClicked → HandleConfirmClicked
  ├─ CancelButton->OnClicked → HandleCancelClicked
  ├─ CloseButton->OnClicked → HandleCancelClicked
  ├─ UpdatePreview()  // 初始显示 "8 × 8 = 64 格"
  └─ WidthSpinBox->SetKeyboardFocus()  // 打开时自动聚焦

HandleWidthChanged(float Value) / HandleHeightChanged(float Value):
  └─ UpdatePreview()

UpdatePreview():
  ├─ int32 W = FMath::RoundToInt(WidthSpinBox->GetValue())
  ├─ int32 H = FMath::RoundToInt(HeightSpinBox->GetValue())
  └─ PreviewText->SetText(FText::Format("{0} × {1} = {2} 格", W, H, W*H))

HandleConfirmClicked():
  └─ OnConfirmed.Broadcast(W, H)

HandleCancelClicked():
  └─ OnCancelled.Broadcast()
```

---

## Widget 3: UValidationResultPanel（验证结果面板）

### 类定义
```cpp
// 验证触发上下文
UENUM(BlueprintType)
enum class EValidationContext : uint8
{
    Save,
    Test
};

UCLASS()
class UValidationResultPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable)
    void Setup(const FLevelValidationResult& Result, EValidationContext Context);

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnValidationAction);

    UPROPERTY(BlueprintAssignable)
    FOnValidationAction OnForceConfirmed;  // "仍然保存/测试"

    UPROPERTY(BlueprintAssignable)
    FOnValidationAction OnClosed;

protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    UScrollBox* ResultList;    // 错误/警告条目容器

    UPROPERTY(meta = (BindWidget))
    UButton* CloseButton;

    UPROPERTY(meta = (BindWidget))
    UButton* ForceButton;      // "仍然保存" / "仍然测试"

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ForceButtonText;

    virtual void NativeConstruct() override;

    void AddResultEntry(const FString& Message, bool bIsError);
};
```

### 布局规格
```
┌──────── 验证结果 ──────────┐
│                            │
│  ✗ 未设置玩家起点          │  ← 红色文本 + ✗ 图标 = Error
│  ✗ 未设置目标格            │
│  ⚠ 门 "Group 1" 没有压力板 │  ← 黄色文本 + ⚠ 图标 = Warning
│  ⚠ 没有箱子生成点          │
│                            │
│  [关闭]  [仍然保存/测试]   │
└────────────────────────────┘
整体: 宽度 400px, 高度自适应（最大 60% 屏幕高度，超出滚动）
```

### 交互逻辑
```
Setup(Result, Context):
  ├─ 清空 ResultList
  ├─ 遍历 Result.Errors:
  │   └─ AddResultEntry(ErrorMsg, bIsError=true)  → 红色 ✗ 前缀
  ├─ 遍历 Result.Warnings:
  │   └─ AddResultEntry(WarningMsg, bIsError=false) → 黄色 ⚠ 前缀
  ├─ if Result.HasErrors():
  │   └─ ForceButton->SetVisibility(ESlateVisibility::Collapsed)  // 有错误时不允许强制操作
  ├─ else (仅警告):
  │   ├─ ForceButton->SetVisibility(ESlateVisibility::Visible)
  │   └─ ForceButtonText->SetText(Context==Save ? "仍然保存" : "仍然测试")
  └─ TitleText->SetText("验证结果")

AddResultEntry(Message, bIsError):
  ├─ 创建 HorizontalBox
  ├─ 添加图标 TextBlock: bIsError ? "✗" (红色 #FF4444) : "⚠" (黄色 #FFAA00)
  ├─ 添加消息 TextBlock: 同色系文本
  └─ 添加到 ResultList ScrollBox

CloseButton->OnClicked → OnClosed.Broadcast()
ForceButton->OnClicked → OnForceConfirmed.Broadcast()
```

---

## 公共弹窗基础设施（供 MainWidget 使用）

这三个弹窗不自行管理显示/隐藏，由 `UEditorMainWidget` 的弹窗管理系统统一控制：
- 弹窗创建时不立即 AddToViewport，而是通过 `MainWidget->ShowDialog(DialogWidget)` 显示
- 弹窗显示时，MainWidget 同时显示全屏半透明遮罩（`#000000` 50% 透明度）
- 遮罩点击 → 触发当前弹窗的取消逻辑
- Esc 键 → 触发当前弹窗的取消逻辑（由 Pawn 的 Esc 处理转发到 MainWidget）
- 同时只能打开一个弹窗

## 验收标准
1. ConfirmDialog: 调用 `Setup()` 后正确显示标题/消息/按钮文本; 点击确定/取消各触发对应 delegate
2. NewLevelDialog: SpinBox 范围 3-30; 实时预览文本更新; 确定时广播正确的 Width/Height
3. ValidationResultPanel: 错误红色、警告黄色; 有 Error 时隐藏 ForceButton; 仅 Warning 时可点击强制操作
4. 三个弹窗都支持 X 按钮关闭
