# Module B: 状态栏（StatusBar）

## 概述
底部状态栏，显示当前编辑器模式、笔刷类型、关卡统计信息和临时消息。纯展示组件，不含复杂交互。

## 前置依赖
- 无（批次 1 的一部分，可与 Module A 并行开发）

## 产出文件
```
Source/TuiXiangZi/UI/Editor/EditorStatusBar.h
Source/TuiXiangZi/UI/Editor/EditorStatusBar.cpp
Content/Blueprints/UI/Editor/WBP_EditorStatusBar.uasset
```

---

## 类定义

```cpp
UCLASS()
class UEditorStatusBar : public UUserWidget
{
    GENERATED_BODY()
public:
    // --- 供 MainWidget 调用的刷新接口 ---
    UFUNCTION(BlueprintCallable)
    void RefreshModeText(EEditorMode Mode);

    UFUNCTION(BlueprintCallable)
    void RefreshBrushText(EEditorBrush Brush);

    UFUNCTION(BlueprintCallable)
    void RefreshStats(int32 CellCount, int32 BoxCount, int32 GroupCount);

    UFUNCTION(BlueprintCallable)
    void ShowTemporaryMessage(const FText& Message, float Duration = 3.0f);

protected:
    // --- BindWidget ---
    UPROPERTY(meta = (BindWidget))
    UTextBlock* ModeText;       // 模式区

    UPROPERTY(meta = (BindWidget))
    UTextBlock* BrushText;      // 笔刷区

    UPROPERTY(meta = (BindWidget))
    UTextBlock* StatsText;      // 统计区

    UPROPERTY(meta = (BindWidget))
    UTextBlock* TempMessageText; // 临时消息区

    virtual void NativeConstruct() override;

    FTimerHandle TempMessageTimerHandle;

    UFUNCTION()
    void ClearTemporaryMessage();

    // 闪烁动画
    UPROPERTY(Transient)
    UWidgetAnimation* BlinkAnimation; // 在 Blueprint 中创建

    bool bIsBlinking = false;
};
```

---

## 布局规格

```
┌──────────────────────────────────────────────────────────────────────┐
│ [普通模式]  │  [当前笔刷: 地板]  │  [格子: 24  箱子: 3  分组: 2]  │  [保存成功 ✓] │
│   模式区    │     笔刷区         │          统计区                 │   消息区      │
│  ≈120px    │    ≈150px          │         Fill                    │   ≈200px      │
└──────────────────────────────────────────────────────────────────────┘
高度: 32px 固定
背景: 深灰色 #1E1E1E
分隔: 竖线 "|" 字符 或 1px 的 Separator Widget, 颜色 #555555
```

**Slate 结构：**
```
HorizontalBox
├─ [SizeBox W=120] → ModeText
├─ [Separator 1px]
├─ [SizeBox W=150] → BrushText
├─ [Separator 1px]
├─ [Fill] → StatsText
├─ [Separator 1px]
└─ [SizeBox W=200] → TempMessageText (右对齐)
```

---

## 交互逻辑

### RefreshModeText(EEditorMode Mode)
```
switch (Mode):
  ├─ Normal:
  │   ├─ ModeText->SetText("普通模式")
  │   ├─ ModeText->SetColorAndOpacity(FLinearColor::White)
  │   └─ 停止闪烁动画
  │
  ├─ PlacingPlatesForDoor:
  │   ├─ ModeText->SetText("放置压力板 — 右键结束")
  │   ├─ ModeText->SetColorAndOpacity(FLinearColor(1.0, 0.85, 0.0))  // 黄色
  │   └─ 播放闪烁动画（如有 BlinkAnimation）
  │       // 闪烁实现: UMG Animation, 0.8s 周期, Alpha 1.0↔0.4 循环
  │
  └─ EditingDoorGroup:
      ├─ ModeText->SetText("编辑分组")
      ├─ ModeText->SetColorAndOpacity(FLinearColor(0.3, 0.6, 1.0))  // 蓝色
      └─ 停止闪烁动画
```

### RefreshBrushText(EEditorBrush Brush)
```
// 笔刷中文名映射
static const TMap<EEditorBrush, FText> BrushNames = {
    {Floor,         "地板"},
    {Wall,          "墙壁"},
    {Ice,           "冰面"},
    {Goal,          "目标"},
    {Door,          "机关门"},
    {PressurePlate, "压力板"},
    {BoxSpawn,      "箱子生成"},
    {PlayerStart,   "玩家起点"},
    {Eraser,        "橡皮擦"}
};

// 笔刷颜色标记
static const TMap<EEditorBrush, FLinearColor> BrushColors = {
    {Floor,         白色},
    {Wall,          灰色 #888888},
    {Ice,           浅蓝 #66CCFF},
    {Goal,          红色 #FF4444},
    {Door,          紫色 #AA66FF},
    {PressurePlate, 橙色 #FF8800},
    {BoxSpawn,      橙黄 #FFAA00},
    {PlayerStart,   绿色 #44FF44},
    {Eraser,        红色 #FF4444}
};

BrushText->SetText(FText::Format("笔刷: {0}", BrushNames[Brush]))
BrushText->SetColorAndOpacity(BrushColors[Brush])
```

### RefreshStats(int32 CellCount, int32 BoxCount, int32 GroupCount)
```
StatsText->SetText(FText::Format("格子: {0}  箱子: {1}  分组: {2}",
    CellCount, BoxCount, GroupCount))
StatsText->SetColorAndOpacity(FLinearColor(0.7, 0.7, 0.7))  // 浅灰
```

### ShowTemporaryMessage(FText Message, float Duration)
```
TempMessageText->SetText(Message)
TempMessageText->SetColorAndOpacity(FLinearColor(0.5, 1.0, 0.5))  // 浅绿色（成功类消息）

// 清除上一个 timer（如有）
GetWorld()->GetTimerManager().ClearTimer(TempMessageTimerHandle)

// 设置新 timer
GetWorld()->GetTimerManager().SetTimer(
    TempMessageTimerHandle,
    this, &UEditorStatusBar::ClearTemporaryMessage,
    Duration, false)

ClearTemporaryMessage():
  └─ TempMessageText->SetText(FText::GetEmpty())
```

### NativeConstruct()
```
NativeConstruct():
  ├─ RefreshModeText(EEditorMode::Normal)
  ├─ RefreshBrushText(EEditorBrush::Floor)
  ├─ RefreshStats(0, 0, 0)
  └─ TempMessageText->SetText(FText::GetEmpty())
```

---

## 调用时机（由 MainWidget 负责触发）

| 事件 | 调用 |
|------|------|
| GameMode.OnBrushChanged | `RefreshBrushText(NewBrush)` |
| GameMode.OnEditorModeChanged | `RefreshModeText(NewMode)` |
| 每次 PaintAtGrid / EraseAtGrid 后 | `RefreshStats(GetCellCount(), GetBoxCount(), GetGroupCount())` |
| SaveLevel 成功 | `ShowTemporaryMessage("保存成功: {FileName}", 3.0f)` |
| LoadLevel 成功 | `ShowTemporaryMessage("加载成功: {FileName}", 3.0f)` |
| PressurePlate 未选分组 | `ShowTemporaryMessage("请先在右侧面板中选择一个分组", 3.0f)` |
| PressurePlate 无分组 | `ShowTemporaryMessage("请先放置一扇门", 3.0f)` |

---

## 验收标准
1. 模式切换时文本和颜色正确更新
2. PlacingPlatesForDoor 模式下文本闪烁
3. 笔刷切换时显示中文名和对应颜色
4. 统计数字实时反映关卡状态
5. 临时消息显示后自动清除，新消息覆盖旧消息
