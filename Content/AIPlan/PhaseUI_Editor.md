# 编辑器 UI 实现计划（交互逻辑细化版）

## Context
关卡编辑器的后端逻辑（ALevelEditorGameMode、ALevelEditorPawn、EditorGridVisualizer）已实现完成，但缺少所有 UI Widget。需要设计并实现完整的编辑器 UI 系统。

### 后端已提供的 API 一览
- **Delegates:** `OnBrushChanged(EEditorBrush)`, `OnEditorModeChanged(EEditorMode)`, `OnGroupCreated(int32)`, `OnGroupDeleted(int32)`
- **Brush 枚举:** Floor, Wall, Ice, Goal, Door, PressurePlate, BoxSpawn, PlayerStart, Eraser（共 9 种）
- **Mode 枚举:** Normal, PlacingPlatesForDoor, EditingDoorGroup
- **关卡操作:** `NewLevel(W,H)`, `SaveLevel(FileName)`, `LoadLevel(FileName)`, `TestCurrentLevel()`
- **分组操作:** `CreateNewGroup()→int32`, `DeleteGroup(Id)`, `SetCurrentGroupId(Id)`, `GetAllGroupIds()`, `GetGroupStyle(Id)→FMechanismGroupStyleData`, `SetGroupColor(Id, Base, Active)`
- **状态查询:** `GetStatusText()`, `GetCellCount()`, `GetBoxCount()`, `GetGroupCount()`, `GetCurrentBrush()`, `GetEditorMode()`, `GetCurrentGroupId()`
- **验证/安全:** `ValidateLevel()→FLevelValidationResult`, `ShouldConfirmErase(Pos)`, `GetEraseWarning(Pos)`

---

## Widget 清单（11 个 C++ 类）

| # | Widget | 职责 |
|---|--------|------|
| 1 | UEditorMainWidget | 根容器，持有所有子面板引用，管理弹窗层 |
| 2 | UEditorSidebarWidget | 左侧笔刷选择面板（9 个按钮） |
| 3 | UEditorToolbarWidget | 顶部工具栏（新建/保存/加载/测试/返回） |
| 4 | UNewLevelDialog | 新建关卡弹窗（宽高 SpinBox） |
| 5 | UConfirmDialog | 通用确认/警告弹窗 |
| 6 | UGroupManagerPanel | 右侧分组管理面板 |
| 7 | UGroupEntryWidget | 分组列表单行条目 |
| 8 | UColorPickerPopup | HSV 颜色选择器弹窗 |
| 9 | UStyleSelectorWidget | 样式选择器（Phase 5 可选） |
| 10 | UEditorStatusBar | 底部状态栏 |
| 11 | UValidationResultPanel | 验证结果面板 |

---

## 布局
```
┌─────────────────────────────────────────┐
│  [Toolbar: New|Save|Load|Test|Back]     │  ← 48px 固定高度
├──────┬──────────────────────┬───────────┤
│Brush │                      │  Group    │
│Panel │    Viewport          │  Manager  │
│200px │    (穿透点击)         │  250px    │
│      │                      │           │
├──────┴──────────────────────┴───────────┤
│  [StatusBar: mode | brush | stats]      │  ← 32px 固定高度
└─────────────────────────────────────────┘
     ↑ 弹窗（Dialog/Popup）居中覆盖于 Viewport 区域
```

### 点击穿透规则
- Sidebar、Toolbar、StatusBar、GroupManager：**消耗** 鼠标事件（`SetVisibility(ESlateVisibility::Visible)`）
- 中央 Viewport 区域：**不消耗** 鼠标事件（该区域不放 Widget，或用 `HitTestInvisible`），让点击穿透到 Pawn 的 RaycastToGrid
- 弹窗打开时：全屏半透明背景 **消耗** 所有点击，阻止 Viewport 交互

---

## 通信架构

```
                    ┌─────────────────┐
                    │  GameMode       │
                    │  (数据 + 逻辑)   │
                    └──┬──────────┬───┘
            Delegates ↓          ↑ BlueprintCallable
                    ┌──┴──────────┴───┐
                    │ EditorMainWidget │
                    │  (事件路由中心)    │
                    └──┬──┬──┬──┬──┬──┘
                       ↓  ↓  ↓  ↓  ↓
                 Sidebar Toolbar GroupMgr StatusBar Dialogs
```

**规则：**
1. 子面板 **不持有** GameMode 指针，所有请求通过 MainWidget 的公共方法中转
2. MainWidget 在 `NativeConstruct` 中绑定 GameMode 的 4 个 Delegate
3. Delegate 回调中，MainWidget 调用子面板的 `Refresh*()` 方法更新显示
4. 子面板的按钮 OnClicked → 调用自身声明的 `FOn*` delegate → MainWidget 监听并调用 GameMode

---

## 各 Widget 交互逻辑详细设计

### 1. UEditorMainWidget（根容器）

**职责：** 持有所有子面板引用，绑定 GameMode delegate，管理弹窗显示/隐藏。

**生命周期：**
```
NativeConstruct()
  ├─ 获取 GameMode 引用 (Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode()))
  ├─ 绑定 GameMode Delegates:
  │   ├─ OnBrushChanged      → HandleBrushChanged(EEditorBrush)
  │   ├─ OnEditorModeChanged → HandleModeChanged(EEditorMode)
  │   ├─ OnGroupCreated      → HandleGroupCreated(int32)
  │   └─ OnGroupDeleted      → HandleGroupDeleted(int32)
  ├─ 绑定子面板 Delegates:
  │   ├─ Sidebar->OnBrushSelected    → RequestSetBrush(EEditorBrush)
  │   ├─ Toolbar->OnNewClicked       → ShowNewLevelDialog()
  │   ├─ Toolbar->OnSaveClicked      → RequestSave()
  │   ├─ Toolbar->OnLoadClicked      → RequestLoad()
  │   ├─ Toolbar->OnTestClicked      → RequestTest()
  │   ├─ Toolbar->OnBackClicked      → RequestBack()
  │   ├─ GroupMgr->OnDeleteGroup     → RequestDeleteGroup(int32)
  │   ├─ GroupMgr->OnEditGroupColor  → ShowColorPicker(int32)
  │   └─ GroupMgr->OnSelectGroup     → RequestSelectGroup(int32)
  └─ 初始刷新所有子面板
```

**弹窗管理：**
```
ShowDialog(UUserWidget* Dialog)
  ├─ 将 Dialog 添加到 CanvasPanel 的弹窗层（ZOrder = 100）
  ├─ 显示半透明遮罩（背景点击 → 关闭弹窗）
  └─ 设置 bIsDialogOpen = true

CloseDialog()
  ├─ 移除当前弹窗
  ├─ 隐藏遮罩
  └─ 设置 bIsDialogOpen = false
```

**GameMode Delegate 处理：**
```
HandleBrushChanged(EEditorBrush NewBrush):
  ├─ Sidebar->SetActiveBrush(NewBrush)        // 高亮对应按钮
  └─ StatusBar->RefreshBrushText(NewBrush)     // 更新状态栏笔刷名

HandleModeChanged(EEditorMode NewMode):
  ├─ StatusBar->RefreshModeText(NewMode)       // 更新状态栏模式名
  ├─ if NewMode == PlacingPlatesForDoor:
  │   └─ Sidebar->SetEnabled(false)            // 禁用笔刷切换
  ├─ if NewMode == Normal:
  │   └─ Sidebar->SetEnabled(true)             // 恢复笔刷切换
  └─ GroupMgr->RefreshActiveGroup(GameMode->GetCurrentGroupId())

HandleGroupCreated(int32 GroupId):
  └─ GroupMgr->AddGroupEntry(GroupId, GameMode->GetGroupStyle(GroupId))

HandleGroupDeleted(int32 GroupId):
  └─ GroupMgr->RemoveGroupEntry(GroupId)
```

---

### 2. UEditorSidebarWidget（笔刷面板）

**布局：** 垂直排列 9 个笔刷按钮，每个按钮包含图标 + 文字标签 + 快捷键提示。

**按钮列表：**
| 按钮 | Brush 枚举 | 快捷键 | 图标描述 |
|------|-----------|--------|---------|
| 地板 | Floor | 1 | 白色方块 |
| 墙壁 | Wall | 2 | 灰色实心方块 |
| 冰面 | Ice | 3 | 蓝色方块 |
| 目标 | Goal | 4 | 红旗 |
| 机关门 | Door | 5 | 门 |
| 压力板 | PressurePlate | 6 | 按钮 |
| 箱子生成 | BoxSpawn | 7 | 箱子 |
| 玩家起点 | PlayerStart | 8 | 人形 |
| 橡皮擦 | Eraser | 0/E | × |

**交互逻辑：**
```
用户点击笔刷按钮:
  ├─ 广播 OnBrushSelected(EEditorBrush)
  └─ MainWidget 收到后 → GameMode->SetCurrentBrush(Brush)
      └─ GameMode 广播 OnBrushChanged
          └─ MainWidget → Sidebar->SetActiveBrush(Brush)

SetActiveBrush(EEditorBrush Brush):
  ├─ 遍历所有按钮，取消高亮
  └─ 找到匹配的按钮，添加高亮边框（选中态：亮蓝色边框 + 浅蓝色背景）

SetEnabled(bool bEnabled):
  ├─ true:  所有按钮恢复交互，正常颜色
  └─ false: 所有按钮禁用交互，灰色显示
      （PlacingPlatesForDoor 模式下，不允许切换笔刷）
```

**特殊交互 — Door 笔刷：**
```
用户选择 Door 笔刷:
  └─ 正常流程（SetCurrentBrush → Door）

用户在 Viewport 点击放置 Door:
  └─ GameMode.PaintAtGrid() 内部:
      ├─ CreateNewGroup() → 创建新分组
      ├─ SetEditorMode(PlacingPlatesForDoor) → 进入压力板放置模式
      └─ 广播 OnEditorModeChanged + OnGroupCreated

此时 UI 反应:
  ├─ Sidebar 被禁用（灰色）
  ├─ StatusBar 显示 "放置压力板模式 — 右键结束"
  └─ GroupManager 新增一个分组条目

用户右键:
  └─ Pawn 检测到 PlacingPlatesForDoor 模式 → 调用 CancelPlacementMode()
      └─ 恢复 Normal 模式 → Sidebar 重新启用
```

**特殊交互 — PressurePlate 笔刷：**
```
用户选择 PressurePlate 笔刷（在 Normal 模式下）:
  ├─ 如果没有任何分组 → 提示 "请先放置一扇门"（通过 StatusBar 或 Toast）
  └─ 如果有分组 → 需要先在 GroupManager 中选择目标分组
      └─ 设置 CurrentGroupId 后才能放置
```

---

### 3. UEditorToolbarWidget（工具栏）

**布局：** 水平排列 5 个按钮，左对齐。

| 按钮 | 图标 | 快捷键 | Delegate |
|------|------|--------|----------|
| 新建 | 📄 | Ctrl+N | OnNewClicked |
| 保存 | 💾 | Ctrl+S | OnSaveClicked |
| 加载 | 📂 | Ctrl+O | OnLoadClicked |
| 测试 | ▶️ | F5 | OnTestClicked |
| 返回 | ← | Esc | OnBackClicked |

**各按钮交互流程：**

#### 新建（New）
```
用户点击 "新建":
  └─ MainWidget.ShowNewLevelDialog()
      └─ 显示 NewLevelDialog 弹窗

NewLevelDialog 中:
  ├─ 用户输入宽度 (SpinBox, 范围 3-30, 默认 8)
  ├─ 用户输入高度 (SpinBox, 范围 3-30, 默认 8)
  └─ 点击 "确定":
      ├─ 如果当前关卡有内容 → 先弹出 ConfirmDialog "未保存的更改将丢失，是否继续？"
      │   ├─ 确认 → GameMode->NewLevel(Width, Height)
      │   └─ 取消 → 关闭弹窗，无操作
      └─ 如果当前关卡为空 → 直接 GameMode->NewLevel(Width, Height)

NewLevel 完成后:
  ├─ StatusBar 刷新统计（CellCount=0, BoxCount=0）
  └─ GroupManager 清空所有条目
```

#### 保存（Save）
```
用户点击 "保存":
  ├─ 先执行 GameMode->ValidateLevel()
  │   ├─ HasErrors() == true:
  │   │   └─ 显示 ValidationResultPanel（红色错误列表）
  │   │       └─ 阻止保存
  │   ├─ HasWarnings() == true:
  │   │   └─ 显示 ValidationResultPanel（黄色警告列表 + "仍然保存" 按钮）
  │   │       ├─ 用户点击 "仍然保存" → 继续保存流程
  │   │       └─ 用户点击 "取消" → 关闭，不保存
  │   └─ 无错误无警告 → 直接进入保存流程
  │
  └─ 保存流程:
      ├─ 弹出文件名输入框（或使用系统文件对话框）
      │   └─ 默认文件名: "CustomLevel_001"（自动递增）
      ├─ GameMode->SaveLevel(FileName)
      │   ├─ 成功 → StatusBar 显示 "保存成功: {FileName}" (3秒后清除)
      │   └─ 失败 → ConfirmDialog 显示错误信息
      └─ 注意: SaveLevel 内部会再次 Validate，这里的前置验证是为了 UI 反馈
```

#### 加载（Load）
```
用户点击 "加载":
  ├─ 如果当前关卡有内容 → ConfirmDialog "未保存的更改将丢失，是否继续？"
  │   ├─ 取消 → 无操作
  │   └─ 确认 → 继续
  └─ 弹出文件选择列表（扫描 SaveGames 目录下的 .json 文件）
      ├─ 用户选择文件
      ├─ GameMode->LoadLevel(FileName)
      │   ├─ 成功:
      │   │   ├─ StatusBar 显示 "加载成功: {FileName}"
      │   │   ├─ GroupManager 重建分组列表 (遍历 GetAllGroupIds())
      │   │   └─ Sidebar 重置为 Floor 笔刷
      │   └─ 失败 → ConfirmDialog 显示错误
      └─ 关闭弹窗
```

#### 测试（Test）
```
用户点击 "测试":
  ├─ GameMode->ValidateLevel()
  │   ├─ HasErrors() → 显示 ValidationResultPanel，阻止测试
  │   └─ 无错误 → 继续（警告允许测试）
  └─ GameMode->TestCurrentLevel()
      └─ 保存临时文件 → OpenLevel(GameMap) → 进入游玩模式
```

#### 返回（Back）
```
用户点击 "返回":
  ├─ 如果当前关卡有内容:
  │   └─ ConfirmDialog "未保存的更改将丢失，是否返回主菜单？"
  │       ├─ 确认 → OpenLevel(MainMenuMap)
  │       └─ 取消 → 无操作
  └─ 如果当前关卡为空:
      └─ 直接 OpenLevel(MainMenuMap)
```

---

### 4. UNewLevelDialog（新建关卡弹窗）

**布局：**
```
┌──────────────────────────┐
│  新建关卡          [X]   │
├──────────────────────────┤
│                          │
│  宽度:  [  8  ] [▲][▼]  │
│  高度:  [  8  ] [▲][▼]  │
│                          │
│  预览: 8 × 8 = 64 格    │
│                          │
│    [取消]    [确定]      │
└──────────────────────────┘
```

**交互逻辑：**
```
SpinBox 交互:
  ├─ 范围: 3 ~ 30
  ├─ 步长: 1
  ├─ 键盘可直接输入
  └─ OnValueChanged → 实时更新预览文本 "W × H = W*H 格"

确定按钮:
  └─ 广播 OnConfirmed(int32 Width, int32 Height)
      └─ MainWidget 收到后处理保存确认逻辑（见 Toolbar.新建 流程）

取消按钮 / X按钮 / Esc键 / 点击遮罩:
  └─ 关闭弹窗，无操作

打开时:
  └─ SpinBox 获得焦点，方便直接输入
```

---

### 5. UConfirmDialog（通用确认弹窗）

**布局：**
```
┌──────────────────────────┐
│  {Title}           [X]   │
├──────────────────────────┤
│                          │
│  {Message}               │
│                          │
│    [取消]    [确定]      │
└──────────────────────────┘
```

**公共接口：**
```cpp
void Setup(const FText& Title, const FText& Message,
           const FText& ConfirmText = "确定",
           const FText& CancelText = "取消");

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConfirmResult);
FOnConfirmResult OnConfirmed;
FOnConfirmResult OnCancelled;
```

**交互逻辑：**
```
确定按钮 → 广播 OnConfirmed → MainWidget 执行对应操作
取消按钮 / X / Esc / 遮罩点击 → 广播 OnCancelled → 关闭弹窗
```

**使用场景：**
- 删除分组确认: "删除分组 {Name} 将同时移除所有关联的门和压力板，是否继续？"
- 新建/加载时未保存确认: "未保存的更改将丢失，是否继续？"
- 返回主菜单确认: "未保存的更改将丢失，是否返回主菜单？"
- 擦除危险格子确认: 由 `GetEraseWarning(Pos)` 提供消息文本

---

### 6. UGroupManagerPanel（分组管理面板）

**布局：**
```
┌────────── 分组管理 ─────────┐
│  [+ 新建分组]               │
├─────────────────────────────┤
│ ▶ Group 1  [■] [🎨] [🗑]   │  ← 选中态（高亮）
│   Group 2  [■] [🎨] [🗑]   │  ← 普通态
│   Group 3  [■] [🎨] [🗑]   │
│                             │
│          (空列表时)          │
│   放置门以创建新分组         │
└─────────────────────────────┘
```

**交互逻辑：**

```
"新建分组" 按钮:
  └─ 广播 OnRequestNewGroup
      └─ MainWidget → GameMode->CreateNewGroup()
          └─ 触发 OnGroupCreated delegate → 自动添加条目

列表条目交互 (见 GroupEntryWidget):
  ├─ 点击条目行 → 选中该分组
  │   └─ 广播 OnSelectGroup(GroupId)
  │       └─ MainWidget → GameMode->SetCurrentGroupId(GroupId)
  │
  ├─ 颜色预览方块 [■] → 仅显示当前颜色
  │
  ├─ 调色板按钮 [🎨] → 打开 ColorPickerPopup
  │   └─ 广播 OnEditGroupColor(GroupId)
  │       └─ MainWidget → ShowColorPicker(GroupId)
  │
  └─ 删除按钮 [🗑] → 先弹出 ConfirmDialog
      └─ 广播 OnDeleteGroup(GroupId)
          └─ MainWidget → 弹出确认 → GameMode->DeleteGroup(GroupId)

选中态联动:
  ├─ GameMode.OnEditorModeChanged(PlacingPlatesForDoor):
  │   └─ 自动高亮正在编辑的 Group（CurrentGroupId）
  ├─ 用户手动选择其他 Group:
  │   └─ 更新 CurrentGroupId（仅在 Normal 模式下允许切换）
  └─ PlacingPlatesForDoor 模式下:
      └─ 禁止切换选中的分组（当前正在为该分组放置压力板）

AddGroupEntry(int32 GroupId, FMechanismGroupStyleData Style):
  ├─ 创建 UGroupEntryWidget 实例
  ├─ 配置 GroupId、DisplayName、BaseColor
  └─ 添加到 ScrollBox

RemoveGroupEntry(int32 GroupId):
  ├─ 从 ScrollBox 中找到并移除对应条目
  └─ 如果删除的是选中分组 → 清除选中态

RefreshActiveGroup(int32 ActiveGroupId):
  └─ 遍历所有条目，高亮 ActiveGroupId 对应条目
```

---

### 7. UGroupEntryWidget（分组条目）

**布局：** 单行水平排列
```
[▶] Group 1  [■■] [🎨] [🗑]
 ↑    ↑        ↑    ↑    ↑
选中  名称   颜色  编辑  删除
标记        预览  颜色
```

**属性：**
```cpp
int32 GroupId;
FText DisplayName;
FLinearColor BaseColor;
bool bIsSelected;
```

**交互：**
- 整行可点击（选中该分组）
- 悬浮时背景变浅色
- 选中态：左侧 ▶ 标记 + 亮色背景
- 颜色预览块实时反映 BaseColor
- 三个按钮（颜色预览、调色板、删除）各有独立的 OnClicked delegate

---

### 8. UColorPickerPopup（颜色选择器弹窗）

**布局：**
```
┌─────── 选择颜色 ──────────┐
│                            │
│  ┌──────────────┐ ┌──┐    │
│  │              │ │  │    │  ← 左: SV 平面  右: H 滑条
│  │   SV 平面    │ │H │    │
│  │              │ │  │    │
│  └──────────────┘ └──┘    │
│                            │
│  预览: [旧色] → [新色]     │
│  HEX: [#FF6600]           │
│                            │
│    [取消]    [应用]        │
└────────────────────────────┘
```

**交互逻辑：**
```
打开时:
  ├─ 接收参数: GroupId, CurrentBaseColor, CurrentActiveColor
  ├─ 将 CurrentBaseColor 转换为 HSV
  └─ 初始化 SV 平面位置 + H 滑条位置

SV 平面 (Saturation-Value):
  ├─ 鼠标按下/拖拽 → 更新 S 和 V 值
  ├─ X 轴 = Saturation (0~1, 左→右)
  └─ Y 轴 = Value (1~0, 上→下)

H 滑条 (Hue):
  ├─ 鼠标按下/拖拽 → 更新 H 值
  └─ Y 轴 = Hue (0~360, 上→下)

实时预览:
  ├─ 任何 HSV 变化 → 重新计算 RGB
  ├─ 更新 "新色" 预览块
  ├─ 更新 HEX 文本框
  └─ ActiveColor 自动 = BaseColor * 1.3 亮度

HEX 输入框:
  ├─ 用户可手动输入 HEX 值
  ├─ OnTextCommitted → 解析 HEX → 更新 HSV 控件位置
  └─ 无效输入 → 恢复上一个有效值

应用按钮:
  └─ 广播 OnColorConfirmed(GroupId, BaseColor, ActiveColor)
      └─ MainWidget → GameMode->SetGroupColor(GroupId, Base, Active)
          └─ Viewport 中对应门和压力板立即更新颜色

取消:
  └─ 关闭弹窗，颜色不变
```

---

### 9. UEditorStatusBar（状态栏）

**布局：** 水平三段式
```
[Normal 模式]  |  [当前笔刷: Floor]  |  [格子: 24  箱子: 3  分组: 2]  |  [保存成功 ✓]
   模式区           笔刷区               统计区                         消息区(临时)
```

**交互逻辑：**
```
RefreshModeText(EEditorMode Mode):
  ├─ Normal             → "普通模式"（白色）
  ├─ PlacingPlatesForDoor → "放置压力板 — 右键结束"（黄色闪烁）
  └─ EditingDoorGroup   → "编辑分组"（蓝色）

RefreshBrushText(EEditorBrush Brush):
  └─ 显示笔刷中文名 + 对应颜色标记

RefreshStats():
  ├─ 每次 Paint/Erase 后由 MainWidget 调用
  └─ 读取 GameMode 的 GetCellCount(), GetBoxCount(), GetGroupCount()

ShowTemporaryMessage(FText Message, float Duration = 3.0f):
  ├─ 在消息区显示文本（如 "保存成功"）
  ├─ 启动 Timer
  └─ Duration 秒后清除

特殊模式提示:
  └─ PlacingPlatesForDoor 时，模式区文本带闪烁动画，提醒用户当前在特殊模式中
```

---

### 10. UValidationResultPanel（验证结果面板）

**布局：**
```
┌──────── 验证结果 ──────────┐
│                            │
│  ✗ 未设置玩家起点          │  ← 红色 = Error
│  ✗ 未设置目标格            │
│  ⚠ 门 "Group 1" 没有压力板 │  ← 黄色 = Warning
│  ⚠ 没有箱子生成点          │
│                            │
│  [关闭]  [仍然保存/测试]   │  ← 有 Error 时隐藏 "仍然保存"
└────────────────────────────┘
```

**交互逻辑：**
```
Setup(FLevelValidationResult Result, EValidationContext Context):
  ├─ Context = Save 或 Test（影响按钮文字）
  ├─ 遍历 Result.Errors → 添加红色条目
  ├─ 遍历 Result.Warnings → 添加黄色条目
  ├─ HasErrors():
  │   └─ 隐藏 "仍然保存/测试" 按钮，只能关闭
  └─ 仅有 Warnings:
      └─ 显示 "仍然保存" 或 "仍然测试" 按钮

"仍然保存" → 广播 OnForceConfirmed → MainWidget 继续保存流程
"关闭" → 关闭弹窗
```

---

### 11. UGroupManagerPanel 与 Pawn 的擦除确认联动

**交互流程（最复杂的交互之一）：**
```
用户右键点击一个门格子:
  └─ Pawn.HandleErasing()
      └─ Pawn 不直接调用 EraseAtGrid
          ├─ 先调用 GameMode->ShouldConfirmErase(GridPos)
          │   ├─ false → 直接 GameMode->EraseAtGrid(GridPos)
          │   └─ true:
          │       ├─ Pawn 广播 OnEraseNeedsConfirm(GridPos)
          │       └─ MainWidget 收到后:
          │           ├─ FString Warning = GameMode->GetEraseWarning(GridPos)
          │           ├─ 弹出 ConfirmDialog(Warning)
          │           ├─ 确认 → GameMode->EraseAtGrid(GridPos)
          │           └─ 取消 → 无操作
```

> **注意：** 这需要修改 Pawn 的 HandleErasing()，添加确认判断逻辑。当前 Pawn 直接调用 EraseAtGrid，需要在 Pawn 中添加一个 delegate `FOnEraseNeedsConfirm` 供 MainWidget 绑定。

---

## 快捷键系统

由 LevelEditorPawn 的 EnhancedInput 处理，需要新增以下 InputAction：

| 快捷键 | 功能 | 触发方式 |
|--------|------|---------|
| 1-8, 0/E | 选择笔刷 | Started |
| Ctrl+N | 新建关卡 | Started |
| Ctrl+S | 保存关卡 | Started |
| Ctrl+O | 加载关卡 | Started |
| F5 | 测试关卡 | Started |
| Esc | 取消当前模式 / 关闭弹窗 / 返回 | Started |
| Ctrl+Z | 撤销（预留，暂不实现） | — |

**Esc 键优先级：**
```
Esc 按下:
  ├─ 优先级 1: 如果有弹窗打开 → 关闭弹窗
  ├─ 优先级 2: 如果在 PlacingPlatesForDoor 模式 → CancelPlacementMode()
  └─ 优先级 3: 否则 → 触发 "返回" 流程（带未保存确认）
```

---

## 需要修改的已有代码

### LevelEditorPawn 修改清单
1. **添加 UI 创建逻辑：** 在 BeginPlay 中创建 EditorMainWidget 并 AddToViewport
2. **添加擦除确认：** HandleErasing() 中检查 ShouldConfirmErase，广播 delegate
3. **添加快捷键 InputAction：** 数字键选笔刷、Ctrl 组合键、Esc 多级处理
4. **添加弹窗状态检查：** 当 MainWidget.bIsDialogOpen == true 时，屏蔽所有 Viewport 输入（不绘制、不擦除、不平移）

### LevelEditorGameMode 修改清单
1. **添加脏标记：** `bool bIsDirty` — 任何 Paint/Erase/NewLevel 操作后设为 true，Save 后设为 false
2. **添加脏标记查询：** `bool IsDirty() const` — UI 用来判断是否需要 "未保存" 确认

---

## 实现批次（保持不变，补充依赖说明）

| 批次 | Widget | 前置依赖 | 关键交互验证点 |
|------|--------|---------|--------------|
| 1 | StatusBar + ConfirmDialog + NewLevelDialog | 无 | StatusBar 显示文本; 弹窗打开/关闭/Esc |
| 2 | Sidebar + Toolbar | 批次1（Toolbar 需要 ConfirmDialog 和 NewLevelDialog） | 笔刷选中高亮; 按钮→弹窗流程 |
| 3 | GroupEntry + ColorPicker + GroupManager | 批次2（需要 Sidebar 禁用联动） | 分组 CRUD; 颜色实时预览 |
| 4 | ValidationPanel + EditorMainWidget | 全部子面板 | 完整验证→保存流程; Delegate 全链路 |
| 5 | StyleSelector | Phase 5 可选 | — |
| 6 | Pawn 修改 | 批次4（需要 MainWidget 接口） | 快捷键; 擦除确认; 弹窗屏蔽输入 |
