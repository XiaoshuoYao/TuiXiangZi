# Module C: 笔刷面板（Sidebar）

## 概述
左侧面板，包含 9 个笔刷按钮和 Variant（样式变体）子面板。用户通过此面板选择绘制工具和样式。

## 前置依赖
- 无编译依赖（可与 A/B/D/E 并行开发）
- 运行时由 MainWidget 绑定 delegate

## 产出文件
```
Source/TuiXiangZi/UI/Editor/EditorSidebarWidget.h
Source/TuiXiangZi/UI/Editor/EditorSidebarWidget.cpp
Content/Blueprints/UI/Editor/WBP_EditorSidebar.uasset
```

---

## 类定义

```cpp
UCLASS()
class UEditorSidebarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // --- 供 MainWidget 调用 ---
    UFUNCTION(BlueprintCallable)
    void SetActiveBrush(EEditorBrush Brush);

    UFUNCTION(BlueprintCallable)
    void SetEnabled(bool bEnabled);  // PlacingPlatesForDoor 时禁用

    void InitializeWithCatalog(UTileStyleCatalog* Catalog);

    // --- Delegates（供 MainWidget 绑定）---
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrushSelected, EEditorBrush, Brush);
    UPROPERTY(BlueprintAssignable)
    FOnBrushSelected OnBrushSelected;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVariantSelected, FName, StyleId);
    UPROPERTY(BlueprintAssignable)
    FOnVariantSelected OnVariantSelected;

protected:
    // --- BindWidget ---
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* BrushButtonContainer;  // 9 个笔刷按钮

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* VariantPanel;          // 样式变体子面板

    UPROPERTY(meta = (BindWidget))
    UTextBlock* VariantTitle;            // "样式变体" 标题

    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* VariantGrid;      // 缩略图网格（2 列）

    virtual void NativeConstruct() override;

    // --- 内部状态 ---
    UPROPERTY()
    TArray<UButton*> BrushButtons;       // 9 个按钮引用

    UPROPERTY()
    TArray<UBorder*> BrushButtonBorders; // 按钮外框（高亮用）

    EEditorBrush CurrentBrush = EEditorBrush::Floor;

    UPROPERTY()
    UTileStyleCatalog* TileStyleCatalog;

    // per-brush 记忆上次选择的变体
    TMap<EEditorBrush, FName> LastSelectedVariant;

    FName CurrentVisualStyleId = NAME_None;

    // --- 内部方法 ---
    void CreateBrushButtons();
    void RefreshVariantPanel(EEditorBrush Brush);
    void SelectVariant(FName StyleId);

    UFUNCTION()
    void HandleBrushButtonClicked(EEditorBrush Brush);

    // EEditorBrush → EGridCellType 映射
    static EGridCellType BrushToCellType(EEditorBrush Brush);
};
```

---

## 布局规格

```
┌──────── 笔刷面板 ────────┐   宽度: 200px 固定
│ ┌──────────────────────┐ │
│ │ ▣  地板         [1]  │ │   ← 选中态: 亮蓝色边框 + 浅蓝背景
│ └──────────────────────┘ │
│ ┌──────────────────────┐ │
│ │    墙壁         [2]  │ │   ← 普通态: 深灰背景
│ └──────────────────────┘ │
│ ┌──────────────────────┐ │
│ │    冰面         [3]  │ │
│ └──────────────────────┘ │
│ │ ... 共 9 个 ...       │ │
│                          │
│ ┌──────────────────────┐ │   ← Separator
│ │    样式变体           │ │
│ ├──────────────────────┤ │
│ │ ┌────┐ ┌────┐       │ │   ← 2 列 UniformGridPanel
│ │ │缩略│ │缩略│       │ │
│ │ │ 图 │ │ 图 │       │ │
│ │ ├────┤ ├────┤       │ │
│ │ │名称│ │名称│       │ │
│ │ └────┘ └────┘       │ │
│ └──────────────────────┘ │
└──────────────────────────┘
背景: #252525, 左侧面板整体
```

### 单个笔刷按钮布局
```
Border (圆角 4px)
└─ HorizontalBox
   ├─ [Image 24x24] 图标（纯色方块或简易图标）
   ├─ [Spacer 8px]
   ├─ [TextBlock Fill] 笔刷名称（如 "地板"）
   └─ [TextBlock 24px] 快捷键提示（如 "1"），右对齐，灰色
按钮高度: 36px
间距: 2px
```

### 单个变体缩略图布局
```
Border (选中时亮蓝边框 2px)
└─ VerticalBox
   ├─ [Image 72x72] Thumbnail 纹理
   └─ [TextBlock] DisplayName, 居中, 字号 10
整体尺寸: 80x96
```

---

## 交互逻辑

### 笔刷按钮

**按钮配置表：**
| 索引 | Brush 枚举 | 名称 | 快捷键 | 图标颜色 |
|------|-----------|------|--------|---------|
| 0 | Floor | 地板 | 1 | 白色 #FFFFFF |
| 1 | Wall | 墙壁 | 2 | 灰色 #888888 |
| 2 | Ice | 冰面 | 3 | 浅蓝 #66CCFF |
| 3 | Goal | 目标 | 4 | 红色 #FF4444 |
| 4 | Door | 机关门 | 5 | 紫色 #AA66FF |
| 5 | PressurePlate | 压力板 | 6 | 橙色 #FF8800 |
| 6 | BoxSpawn | 箱子生成 | 7 | 橙黄 #FFAA00 |
| 7 | PlayerStart | 玩家起点 | 8 | 绿色 #44FF44 |
| 8 | Eraser | 橡皮擦 | E | 红色 #FF4444 |

```
CreateBrushButtons():
  └─ for each (Brush, Name, Shortcut, Color):
      ├─ 在 C++ 中创建 Button + Border 结构
      │   (或在 Blueprint 中预设好 9 个按钮，C++ 只绑定事件)
      ├─ 设置图标颜色 = Color
      ├─ 设置名称文本 = Name
      ├─ 设置快捷键文本 = Shortcut
      ├─ Button->OnClicked → HandleBrushButtonClicked(Brush)
      ├─ 存入 BrushButtons[] 和 BrushButtonBorders[]
      └─ 默认: 第一个按钮（Floor）为选中态

HandleBrushButtonClicked(EEditorBrush Brush):
  └─ OnBrushSelected.Broadcast(Brush)
      // 不直接更新高亮，等待 GameMode delegate 回调后由 SetActiveBrush 更新
```

### SetActiveBrush(EEditorBrush Brush)
```
SetActiveBrush(Brush):
  ├─ CurrentBrush = Brush
  │
  ├─ 更新按钮高亮:
  │   for i in 0..8:
  │     if BrushButtons[i] 对应 Brush:
  │       BrushButtonBorders[i]->SetBrushColor(亮蓝色 #4499FF)
  │       BrushButtonBorders[i]->SetBackground(浅蓝 #1A3355, 30% 透明度)
  │     else:
  │       BrushButtonBorders[i]->SetBrushColor(透明)
  │       BrushButtonBorders[i]->SetBackground(深灰 #333333)
  │
  └─ RefreshVariantPanel(Brush)
```

### SetEnabled(bool bEnabled)
```
SetEnabled(bEnabled):
  ├─ for each Button in BrushButtons:
  │   └─ Button->SetIsEnabled(bEnabled)
  │
  ├─ if !bEnabled:
  │   └─ 整个面板设置灰色覆盖 (SetRenderOpacity(0.5))
  └─ else:
      └─ 恢复正常 (SetRenderOpacity(1.0))
```

---

### Variant 子面板

**EEditorBrush → EGridCellType 映射：**
```cpp
static EGridCellType BrushToCellType(EEditorBrush Brush)
{
    switch (Brush)
    {
        case EEditorBrush::Floor:         return EGridCellType::Floor;
        case EEditorBrush::Wall:          return EGridCellType::Wall;
        case EEditorBrush::Ice:           return EGridCellType::Ice;
        case EEditorBrush::Goal:          return EGridCellType::Goal;
        case EEditorBrush::Door:          return EGridCellType::Door;
        case EEditorBrush::PressurePlate: return EGridCellType::PressurePlate;
        default:                          return EGridCellType::Empty; // 无变体
    }
}
```

**RefreshVariantPanel(EEditorBrush Brush):**
```
RefreshVariantPanel(Brush):
  ├─ EGridCellType CellType = BrushToCellType(Brush)
  │
  ├─ if CellType == Empty || TileStyleCatalog == nullptr:
  │   └─ VariantPanel->SetVisibility(Collapsed)  // BoxSpawn/PlayerStart/Eraser
  │      return
  │
  ├─ TArray<const FTileVisualStyle*> Styles = TileStyleCatalog->GetStylesForType(CellType)
  │
  ├─ if Styles.Num() <= 1:
  │   ├─ VariantPanel->SetVisibility(Collapsed)
  │   ├─ CurrentVisualStyleId = NAME_None  // 使用默认
  │   └─ return
  │
  ├─ VariantPanel->SetVisibility(Visible)
  ├─ VariantGrid->ClearChildren()
  │
  ├─ 确定初始选中:
  │   ├─ if LastSelectedVariant.Contains(Brush):
  │   │   └─ SelectedId = LastSelectedVariant[Brush]
  │   └─ else:
  │       └─ SelectedId = Styles[0]->StyleId  // 默认第一个
  │
  ├─ for (int32 i = 0; i < Styles.Num(); i++):
  │   ├─ 创建缩略图 Widget:
  │   │   ├─ Border → VerticalBox → [Image + TextBlock]
  │   │   ├─ Image: 设置 Brush from Styles[i]->Thumbnail (RenderTarget → Brush)
  │   │   ├─ TextBlock: Styles[i]->DisplayName
  │   │   └─ Tooltip: Styles[i]->DisplayName（完整名，防截断）
  │   │
  │   ├─ 添加到 VariantGrid (Row = i/2, Column = i%2)
  │   │
  │   ├─ 绑定点击:
  │   │   └─ OnMouseButtonDown → SelectVariant(Styles[i]->StyleId)
  │   │
  │   └─ if Styles[i]->StyleId == SelectedId:
  │       └─ 设置选中高亮（亮蓝色边框）
  │
  └─ SelectVariant(SelectedId)  // 触发初始选中

SelectVariant(FName StyleId):
  ├─ CurrentVisualStyleId = StyleId
  ├─ LastSelectedVariant.Add(CurrentBrush, StyleId)  // 记忆
  │
  ├─ 更新 VariantGrid 中所有缩略图的边框:
  │   ├─ 匹配的 → 亮蓝色边框 2px (#4499FF)
  │   └─ 其他 → 透明边框
  │
  └─ OnVariantSelected.Broadcast(StyleId)
```

**InitializeWithCatalog(UTileStyleCatalog* Catalog):**
```
InitializeWithCatalog(Catalog):
  ├─ TileStyleCatalog = Catalog
  └─ RefreshVariantPanel(CurrentBrush)  // 初始化变体面板
```
由 MainWidget 在 NativeConstruct 中调用，传入 GameMode/GridManager 持有的 TileStyleCatalog 引用。

---

## 需要新增的 GameMode 接口

此模块需要 GameMode 提供以下新接口（在 Module G 中实现）：

```cpp
// 已有（无需修改）
void SetCurrentBrush(EEditorBrush NewBrush);
EEditorBrush GetCurrentBrush() const;
FOnBrushChanged OnBrushChanged;

// 需新增
UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
FName CurrentVisualStyleId = NAME_None;

UFUNCTION(BlueprintCallable)
void SetCurrentVisualStyleId(FName NewStyleId);

UFUNCTION(BlueprintCallable)
FName GetCurrentVisualStyleId() const;
```

---

## 验收标准
1. 9 个笔刷按钮正确显示名称、图标颜色、快捷键提示
2. 点击按钮广播 OnBrushSelected，SetActiveBrush 正确更新高亮
3. SetEnabled(false) 时所有按钮禁用 + 灰色覆盖
4. 切换到有多变体的笔刷时，Variant 子面板展开，显示缩略图网格
5. 切换到无变体的笔刷时，Variant 子面板收起
6. 选择变体时广播 OnVariantSelected，记忆 per-brush 选择
7. 切回某笔刷时恢复上次选择的变体
