# Phase 4: UE 编辑器内手动操作清单

## 概述
所有 C++ 代码已完成，以下是需要在 Unreal Editor 中手动完成的全部操作。
建议按照本文档的顺序执行，因为后面的 Widget 依赖前面的。

---

## 一、创建材质 (1 个)

### M_SVPlane（SV 色彩平面材质）
- **路径建议：** `Content/Materials/UI/M_SVPlane`
- **用途：** ColorPickerPopup 的饱和度-明度渐变平面
- **类型：** Material，Domain = User Interface
- **参数：**
  - 创建一个 `ScalarParameter` 名为 `Hue`（范围 0~360）
  - 材质逻辑：根据 Hue + UV 坐标计算 HSV→RGB
    - U 轴 = Saturation (0→1)
    - V 轴 = Value (1→0，即顶部亮底部暗)
  - 输出到 Final Color
- **代码对接：** `ColorPickerPopup` 的 `SVPlaneMaterial` 属性，C++ 中会 `UMaterialInstanceDynamic::Create` 并通过 `SetScalarParameterValue("Hue", ...)` 更新色相

---

## 二、创建 Blueprint Widget (10 个)

### 创建顺序建议
先创建无依赖的叶子 Widget，再创建容器 Widget：
1. ConfirmDialog
2. NewLevelDialog
3. ValidationResultPanel
4. EditorStatusBar
5. EditorSidebarWidget
6. EditorToolbarWidget
7. GroupEntryWidget
8. ColorPickerPopup
9. GroupManagerPanel（引用 GroupEntryWidget）
10. EditorMainWidget（引用以上所有）

---

### WBP_ConfirmDialog
- **路径：** `Content/Blueprints/UI/Editor/WBP_ConfirmDialog`
- **Parent Class：** `UConfirmDialog`
- **BindWidget 控件（6 个，名称必须精确匹配）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `TitleText` | TextBlock | 弹窗标题 |
| `MessageText` | TextBlock | 弹窗正文 |
| `ConfirmButton` | Button | 确认按钮 |
| `CancelButton` | Button | 取消按钮 |
| `ConfirmButtonText` | TextBlock | 确认按钮上的文字 |
| `CancelButtonText` | TextBlock | 取消按钮上的文字 |

- **布局建议：**
  - 居中卡片式布局（约 400×250）
  - 顶部 TitleText（加粗/大号）
  - 中部 MessageText
  - 底部水平排列 ConfirmButton + CancelButton

---

### WBP_NewLevelDialog
- **路径：** `Content/Blueprints/UI/Editor/WBP_NewLevelDialog`
- **Parent Class：** `UNewLevelDialog`
- **BindWidget 控件（5 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `WidthSpinBox` | SpinBox | 宽度输入（整数，范围 3-30，默认 8）|
| `HeightSpinBox` | SpinBox | 高度输入（整数，范围 3-30，默认 8）|
| `PreviewText` | TextBlock | 实时预览文字，如 "8 × 8" |
| `ConfirmButton` | Button | 确认创建 |
| `CancelButton` | Button | 取消 |

- **SpinBox 配置：**
  - Min Value: 3, Max Value: 30
  - Min Slider Value: 3, Max Slider Value: 30
  - Delta: 1

---

### WBP_ValidationResultPanel
- **路径：** `Content/Blueprints/UI/Editor/WBP_ValidationResultPanel`
- **Parent Class：** `UValidationResultPanel`
- **BindWidget 控件（5 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `TitleText` | TextBlock | 标题（"验证结果"）|
| `ResultList` | ScrollBox | 动态填充错误/警告条目 |
| `CloseButton` | Button | 关闭 |
| `ForceButton` | Button | 强制执行按钮（有错误时隐藏）|
| `ForceButtonText` | TextBlock | 按钮文字（"仍然保存"/"仍然测试"）|

- **布局建议：**
  - 较大面板（约 500×400）
  - ResultList 占据主要区域，可滚动
  - 底部 ForceButton + CloseButton

---

### WBP_EditorStatusBar
- **路径：** `Content/Blueprints/UI/Editor/WBP_EditorStatusBar`
- **Parent Class：** `UEditorStatusBar`
- **BindWidget 控件（4 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `ModeText` | TextBlock | 当前模式（左侧）|
| `BrushText` | TextBlock | 当前笔刷（左侧第二）|
| `StatsText` | TextBlock | 统计信息（右侧）|
| `TempMessageText` | TextBlock | 临时消息（中间/右侧，默认隐藏）|

- **Widget Animation（1 个）：**
  - 创建名为 `BlinkAnimation` 的 Widget Animation
  - 对 `ModeText` 做透明度闪烁（Opacity 1→0→1 循环）
  - 时长约 0.8~1.0 秒
  - **注意：** C++ 中通过变量名 `BlinkAnimation` 查找，需确保动画变量名精确匹配
  - 也可以在 C++ 的 `NativeConstruct` 中用 `GetAnimationByName` 或直接在 Blueprint 中设置 `BlinkAnimation` 属性引用

- **布局建议：**
  - 水平排列，固定在屏幕底部
  - 背景半透明深色
  - 高度约 30-40px

---

### WBP_EditorSidebar
- **路径：** `Content/Blueprints/UI/Editor/WBP_EditorSidebar`
- **Parent Class：** `UEditorSidebarWidget`
- **BindWidget 控件（4 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `BrushButtonContainer` | VerticalBox | 笔刷按钮容器（C++ 动态创建 9 个按钮）|
| `VariantPanel` | VerticalBox | 变体选择面板区域 |
| `VariantTitle` | TextBlock | 变体标题（如 "地板样式"）|
| `VariantGrid` | UniformGridPanel | 变体缩略图网格（C++ 动态填充）|

- **布局建议：**
  - 垂直排列，固定在屏幕左侧
  - BrushButtonContainer 在上方（9 个笔刷按钮由 C++ 动态生成）
  - VariantPanel 在下方（默认隐藏，选择有变体的笔刷时显示）
  - 宽度约 150-200px

---

### WBP_EditorToolbar
- **路径：** `Content/Blueprints/UI/Editor/WBP_EditorToolbar`
- **Parent Class：** `UEditorToolbarWidget`
- **BindWidget 控件（5 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `NewButton` | Button | 新建关卡 |
| `SaveButton` | Button | 保存关卡 |
| `LoadButton` | Button | 加载关卡 |
| `TestButton` | Button | 测试关卡 |
| `BackButton` | Button | 返回主菜单 |

- **布局建议：**
  - 水平排列，固定在屏幕顶部
  - 每个按钮内放 TextBlock 显示文字
  - 背景半透明深色
  - Tooltip 文字：新建(Ctrl+N)、保存(Ctrl+S)、加载(Ctrl+O)、测试(F5)、返回

---

### WBP_GroupEntryWidget
- **路径：** `Content/Blueprints/UI/Editor/WBP_GroupEntryWidget`
- **Parent Class：** `UGroupEntryWidget`
- **BindWidget 控件（6 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `RowButton` | Button | 整行可点击按钮（底层）|
| `SelectionMark` | TextBlock | 选中标记 "▶"（默认隐藏）|
| `NameText` | TextBlock | 分组名称 |
| `ColorPreview` | Image | 颜色预览色块 |
| `ColorEditButton` | Button | 编辑颜色按钮 |
| `DeleteButton` | Button | 删除分组按钮 |

- **布局建议：**
  - 水平排列，高度约 40px
  - RowButton 作为最底层（Overlay 或 Size Box 包裹）
  - 左→右：SelectionMark | NameText | ColorPreview | ColorEditButton | DeleteButton

---

### WBP_ColorPickerPopup
- **路径：** `Content/Blueprints/UI/Editor/WBP_ColorPickerPopup`
- **Parent Class：** `UColorPickerPopup`
- **BindWidget 控件（9 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `SVPlane` | Image | 饱和度-明度平面（约 200×200）|
| `SVCursor` | Image | SV 平面上的选择光标（小圆圈）|
| `HueBar` | Image | 色相条（垂直，约 20×200）|
| `HueCursor` | Image | 色相条上的选择光标（小三角/横线）|
| `OldColorPreview` | Image | 原颜色预览 |
| `NewColorPreview` | Image | 新颜色预览 |
| `HexInput` | EditableTextBox | HEX 颜色输入框 |
| `ApplyButton` | Button | 应用 |
| `CancelButton` | Button | 取消 |

- **额外配置：**
  - SVPlane 的 Image 初始 Brush 可以留空（C++ 会设置 MID）
  - HueBar 需要一个彩虹渐变纹理或材质（从红→黄→绿→青→蓝→紫→红）
  - SVCursor / HueCursor 用小圆点或小三角图标

- **布局建议：**
  - 弹窗卡片式（约 350×350）
  - 左侧 SVPlane + 右侧 HueBar
  - 下方：OldColorPreview | NewColorPreview | HexInput
  - 底部：ApplyButton + CancelButton

---

### WBP_GroupManagerPanel
- **路径：** `Content/Blueprints/UI/Editor/WBP_GroupManagerPanel`
- **Parent Class：** `UGroupManagerPanel`
- **BindWidget 控件（3 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `NewGroupButton` | Button | 新建分组按钮 |
| `GroupList` | ScrollBox | 分组列表容器（C++ 动态添加 GroupEntryWidget）|
| `EmptyHintText` | TextBlock | 空列表提示（"暂无分组"）|

- **TSubclassOf 配置（1 个）：**
  - `GroupEntryWidgetClass` → 设置为 `WBP_GroupEntryWidget`

- **布局建议：**
  - 垂直排列，固定在屏幕右侧
  - 顶部 NewGroupButton
  - 中间 GroupList（可滚动）
  - EmptyHintText 居中显示（有分组时隐藏）
  - 宽度约 200-250px

---

### WBP_EditorMainWidget（最后创建）
- **路径：** `Content/Blueprints/UI/Editor/WBP_EditorMainWidget`
- **Parent Class：** `UEditorMainWidget`
- **BindWidget 控件（6 个）：**

| 控件名 | 类型 | 说明 |
|--------|------|------|
| `Sidebar` | WBP_EditorSidebar (UEditorSidebarWidget) | 左侧栏（作为子 Widget 嵌入）|
| `Toolbar` | WBP_EditorToolbar (UEditorToolbarWidget) | 顶部栏 |
| `GroupManager` | WBP_GroupManagerPanel (UGroupManagerPanel) | 右侧分组面板 |
| `StatusBar` | WBP_EditorStatusBar (UEditorStatusBar) | 底部状态栏 |
| `DialogLayer` | CanvasPanel | 弹窗层（覆盖全屏，默认隐藏）|
| `DialogOverlay` | Button | 弹窗背景遮罩（半透明黑色，点击关闭弹窗）|

- **TSubclassOf 配置（4 个）：**
  - `ConfirmDialogClass` → `WBP_ConfirmDialog`
  - `NewLevelDialogClass` → `WBP_NewLevelDialog`
  - `ValidationPanelClass` → `WBP_ValidationResultPanel`
  - `ColorPickerClass` → `WBP_ColorPickerPopup`

- **布局结构：**
  ```
  [Root - Overlay / CanvasPanel]
  ├─ [主内容层]
  │  ├─ Toolbar        (顶部, Anchor: Top-Stretch)
  │  ├─ Sidebar        (左侧, Anchor: Left-Stretch)
  │  ├─ GroupManager   (右侧, Anchor: Right-Stretch)
  │  ├─ StatusBar      (底部, Anchor: Bottom-Stretch)
  │  └─ (中央留空 — Viewport 区域，不放任何 Widget)
  └─ DialogLayer       (全屏覆盖层, Anchor: Fill, 默认 Collapsed)
     └─ DialogOverlay  (全屏半透明按钮, Anchor: Fill)
         └─ (弹窗由 C++ 动态创建并添加到此处)
  ```

- **重要：** 中央 Viewport 区域**不要放任何 Widget**，确保鼠标事件能穿透到 Pawn 的 RaycastToGrid

---

## 三、配置 Blueprint 默认值

### BP_LevelEditorPawn（已有 Blueprint）
- 打开 `BP_LevelEditorPawn` 的 Class Defaults
- 找到 `MainWidgetClass` 属性
- 设置为 → `WBP_EditorMainWidget`

### WBP_GroupManagerPanel
- 打开 WBP_GroupManagerPanel 的 Class Defaults
- 找到 `GroupEntryWidgetClass` 属性
- 设置为 → `WBP_GroupEntryWidget`

### WBP_EditorMainWidget
- 打开 WBP_EditorMainWidget 的 Class Defaults
- 设置以下 4 个 TSubclassOf 属性：
  - `ConfirmDialogClass` → `WBP_ConfirmDialog`
  - `NewLevelDialogClass` → `WBP_NewLevelDialog`
  - `ValidationPanelClass` → `WBP_ValidationResultPanel`
  - `ColorPickerClass` → `WBP_ColorPickerPopup`

### WBP_ColorPickerPopup
- 打开 WBP_ColorPickerPopup 的 Class Defaults
- 找到 `SVPlaneMaterial` 属性
- 设置为 → `M_SVPlane`（第一步创建的材质）

---

## 四、创建辅助资源

### 4.1 HueBar 纹理/材质
- **用途：** ColorPickerPopup 的色相条背景
- 创建一个垂直彩虹渐变纹理（1px 宽，256px 高）
- 从上到下：红 → 黄 → 绿 → 青 → 蓝 → 紫 → 红
- 设置为 HueBar Image 的 Brush Texture

### 4.2 光标图标
- SVCursor：小圆圈（约 12×12），白色描边
- HueCursor：小三角或横线（约 16×4），白色

### 4.3 按钮图标（可选）
- ColorEditButton 用的调色盘图标
- DeleteButton 用的垃圾桶图标

---

## 五、验证清单

完成以上所有步骤后，按以下顺序验证：

- [ ] **编译通过** — 热编译或完整编译无错误
- [ ] **进入编辑器关卡** — UI 自动显示，布局正确
- [ ] **鼠标可用** — 光标可见，可同时操作 UI 按钮和 Viewport 绘制
- [ ] **笔刷切换** — 点击左侧按钮切换笔刷，StatusBar 同步更新
- [ ] **数字键 1-8/E** — 快捷键切换笔刷
- [ ] **绘制/擦除** — 左键绘制，右键擦除，StatusBar 统计实时刷新
- [ ] **Toolbar 按钮** — 新建/保存/加载/测试/返回 各功能正常
- [ ] **Ctrl+N/S/O, F5** — 快捷键触发对应功能
- [ ] **脏标记** — 修改后新建/加载/返回弹出未保存确认
- [ ] **新建关卡** — SpinBox 输入尺寸，确认后清空地图
- [ ] **保存验证** — 有错误时显示 ValidationResultPanel，阻止保存
- [ ] **分组系统** — 新建/删除/选择/修改颜色
- [ ] **颜色选择器** — SV 平面拖拽、Hue 条拖拽、HEX 输入
- [ ] **擦除确认** — 右键点击门格子弹出确认框
- [ ] **Esc 键** — 三级优先级（弹窗 > 编辑模式 > 无操作）
- [ ] **弹窗屏蔽** — 弹窗打开时 Viewport 操作全部无效
- [ ] **变体选择** — 选择有变体的笔刷时显示缩略图网格

---

## 控件总计

| 类别 | 数量 |
|------|------|
| Blueprint Widget | 10 个 |
| 材质 | 1 个 (M_SVPlane) |
| Widget Animation | 1 个 (BlinkAnimation) |
| BindWidget 控件总数 | 48 个 |
| TSubclassOf 配置 | 6 个 |
| 辅助纹理/图标 | 3~5 个 |
