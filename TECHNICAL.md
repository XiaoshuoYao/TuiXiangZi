# 技术文档

## 技术栈

- **引擎**：Unreal Engine 5.5
- **语言**：C++（单模块 `TuiXiangZi`）+ Blueprint（UI 和视觉资产）
- **关卡数据**：JSON 格式序列化
- **输入系统**：Enhanced Input
- **依赖模块**：Core, CoreUObject, Engine, InputCore, EnhancedInput, Niagara, Json, JsonUtilities, ProceduralMeshComponent, UMG, Slate, SlateCore, RenderCore, NavigationSystem, AIModule

---

## 目录结构

```
Source/TuiXiangZi/
├── Events/          # 事件总线（UWorldSubsystem）
├── Framework/       # GameInstance, GameMode, GameState, SaveGame
├── Grid/            # GridManager, GridTypes, TileStyleCatalog, TileVisualActor
├── Gameplay/        # SokobanCharacter, PushableBoxComponent, GroupColorIndicatorComponent
│   ├── Mechanisms/  # GridMechanismComponent (基类), Door, PressurePlate, Goal, Teleporter
│   └── Modifiers/   # TileModifierComponent (基类), IceTileModifier
├── LevelData/       # LevelDataTypes, LevelSerializer (JSON)
├── Editor/          # LevelEditorGameMode, EditorOverlayManager, LevelEditorPawn, BrushTypes
│   └── Overlays/    # GridLineOverlay, CoordinateLabelOverlay, TeleporterArrowOverlay
├── Tutorial/        # TutorialTypes, TutorialDataAsset, TutorialSubsystem
└── UI/              # MainMenu, LevelSelect, PauseMenu, TutorialWidget, PlayerController
    └── Editor/      # EditorMain, Sidebar, Toolbar, StatusBar, GroupManager, Dialogs
```

---

## 核心子系统

### 1. 事件系统（Events/）

**GameEventBus**（`UWorldSubsystem`）：中央事件总线，所有子系统通过 `FName` 标签 + `FGameEventPayload` 进行解耦通信。

```cpp
// 广播方：一行代码，不关心谁在监听
EventBus->Broadcast(GameEventTags::Player::Moved, FGameEventPayload::FromActor(this, NewPos));

// 订阅方：按标签注册回调
EventBus->Subscribe(GameEventTags::Player::Moved, FOnGameEvent::CreateUObject(this, &ThisClass::OnPlayerMoved));
```

**GameEventPayload**：通用载荷结构体，包含 Actor、GridPos、FromPos、ToPos、IntParam、FloatParam 字段和工厂方法（MakeActorMoved, MakeGridPos, MakeGridBounds 等）。

**GameEventTags**：`inline const FName` 常量命名空间，覆盖所有事件标签：
- `Grid.*`：网格状态变化（ActorMoved, PlayerEnteredGoal, PitFilled）
- `Player.*`：玩家操作（Moved, PushedBox, Undid, Reset）
- `Editor.*`：编辑器操作（CellPainted, BrushChanged, LevelSaved, GridBoundsChanged, TeleporterDirectionChanged）

### 2. 网格系统（Grid/）

**GridManager**：核心协调器，管理网格状态和所有空间逻辑。

- 网格存储：`TMap<FIntPoint, FGridCell>`，支持任意形状的关卡
- 占位管理：跟踪每个格子上的 Actor（玩家、箱子）
- 移动校验：`TryMoveActor()` 统一处理移动请求，检查通行性、推箱子、机关阻挡
- 机制发现：自动检测 TileVisualActor 上的 Mechanism/Modifier 组件
- 事件广播：通过 GameEventBus 广播移动、胜利等事件

**GridTypes**：

```cpp
// 地块类型枚举
enum class EGridCellType : uint8 {
    Empty, Floor, Wall, PressurePlate, Ice, Goal, Door, Box, Teleporter
};

// 描述表：一行定义贯通序列化、通行、渲染、编辑器行为
struct FCellTypeDescriptor {
    EGridCellType Type;
    const TCHAR*  TypeString;              // JSON 序列化键
    bool          bPassable;               // 默认通行性
    bool          bNeedsFloorUnderlay;     // 是否需要地板底层渲染
    bool          bEraseReplacesWithFloor; // 编辑器擦除时是否替换为地板
};
```

**TileVisualActor**：所有网格视觉元素的基类 Actor。Mechanism 和 Modifier 组件挂载于此。

**TileStyleCatalog**：DataAsset，按地块类型配置视觉样式变体。

### 3. 框架层（Framework/）

**SokobanGameInstance**：
- 关卡选择状态（预设/自定义/编辑器）
- 进度追踪与存档管理
- 关卡文件发现

**SokobanGameMode**：
- 游玩编排：关卡加载、撤销、步数计数、胜利检测
- 通过 GridManager 驱动所有游玩逻辑

**SokobanGameState**：
- 运行时状态：步数计数器、通关标记
- 撤销快照栈：`TArray<FLevelSnapshot>`

**SokobanSaveGame**：
- 持久化：已通关关卡集合、最高解锁索引

### 4. 游玩层（Gameplay/）

**SokobanCharacter**：
- 网格 4 方向移动，Enhanced Input 驱动
- Timeline 动画插值（0.15s 时长）
- 俯视角相机

**PushableBoxComponent**：
- 推箱子/掉落逻辑
- 压力板反馈
- 动态材质实例

**GroupColorIndicatorComponent**：
- 可蓝图化的分组颜色显示组件
- BP 子类可重写 `OnUpdateVisual` 自定义视觉效果
- 由 GridManager 和 GridMechanismComponent 自动通知更新

**机制组件**（Mechanisms/）：

| 组件 | 职责 | 关键接口 |
|------|------|----------|
| `GridMechanismComponent` | 抽象基类 | `OnActivate()`, `OnDeactivate()`, `BlocksPassage()`, `IsGroupTrigger()` |
| `DoorMechanismComponent` | 门：关闭时阻挡通行 | `IsCurrentlyBlocking()`, Timeline 开关动画 |
| `PressurePlateMechanismComponent` | 压力板：箱子占位时触发分组 | `IsGroupTrigger() = true` |
| `GoalMechanismComponent` | 目标点：胜利条件标记 | — |
| `TeleporterMechanismComponent` | 传送阵：成对传送 | `CanSend()`, `CanReceive()`, 方向切换 |

**基类接口设计**：

```cpp
class UGridMechanismComponent : public UGridTileComponent {
    // 激活状态
    virtual void OnActivate();
    virtual void OnDeactivate();

    // 通行控制
    virtual bool BlocksPassage() const { return false; }
    virtual bool IsCurrentlyBlocking() const { return false; }

    // 分组角色
    virtual bool IsGroupTrigger() const { return false; }

    // 编辑器放置流程
    virtual EEditorPlacementFlow GetEditorPlacementFlow() const;
    // None = 普通放置, AssignGroup = 自动创建组, PairPlacement = 配对放置
};
```

**修饰组件**（Modifiers/）：

```cpp
class UTileModifierComponent : public UGridTileComponent {
    // 唯一接口：是否继续移动
    virtual bool ShouldContinueMovement(EMoveDirection Direction) const { return false; }
};

// 冰面实现：永远继续滑行
class UIceTileModifier : public UTileModifierComponent {
    bool ShouldContinueMovement(EMoveDirection Direction) const override { return true; }
};
```

GridManager 在移动逻辑中自动检查目标格子的 Modifier，如果 `ShouldContinueMovement()` 返回 true，则通过 `CalculateIceSlideDestination()` 计算最终落点，将整段滑行作为一次原子移动处理。

### 5. 关卡数据（LevelData/）

**LevelDataTypes**：

```cpp
struct FLevelData {
    TArray<FCellData>                Cells;          // 所有格子数据
    FIntPoint                        PlayerStartPos; // 玩家出生点
    TArray<FMechanismGroupStyleData> GroupStyles;     // 分组颜色配置
};

struct FCellData {
    FIntPoint     Position;
    EGridCellType CellType;
    int32         GroupId;     // 机关分组 ID
    int32         ExtraParam;  // 扩展参数（传送阵方向等）
};
```

**LevelSerializer**：JSON 序列化/反序列化，关卡文件发现。序列化键由 `FCellTypeDescriptor::TypeString` 驱动，新增地块类型自动获得序列化支持。

### 6. 编辑器（Editor/）

**LevelEditorGameMode**：
- 笔刷系统：10 种笔刷，描述表驱动
- 编辑模式状态机：普通绘制 / 压力板放置 / 传送阵配对
- 分组管理：创建、删除、颜色自定义
- 关卡验证：保存/测试前自动检查合法性
- 事件广播：通过 GameEventBus 广播编辑器操作（不直接引用任何 overlay 类）

**笔刷描述表**：

```cpp
struct FBrushDescriptor {
    EEditorBrush  Brush;
    EGridCellType CellType;      // 对应地块类型
    const TCHAR*  DisplayName;   // 中文显示名
    const TCHAR*  StatusName;    // 状态栏英文名
    const TCHAR*  Shortcut;      // 快捷键
    FLinearColor  IconColor;     // UI 图标颜色
};
```

**EditorOverlayManager**：管理编辑器可视化覆盖层。拥有三个 overlay 组件，管理 overlay 模式循环（G 键），协调 `FlushPersistentDebugLines` 以避免 debug line 互相擦除。

**EditorOverlayComponent**（抽象基类）：事件驱动的 overlay 组件。每个子类在 `BeginPlay` 中订阅 GameEventBus 事件，收到事件后自行获取数据并重绘。接口：`Rebuild()`, `Clear()`, `SetVisible()`, `RedrawDebugLines()`。

| overlay 组件 | 渲染方式 | 订阅事件 | 受模式控制 |
|-------------|---------|---------|-----------|
| `GridLineOverlay` | DrawDebugLine | `Editor.GridBoundsChanged` | 是 |
| `CoordinateLabelOverlay` | TextRenderActor | `Editor.GridBoundsChanged` | 是 |
| `TeleporterArrowOverlay` | DrawDebugLine | `Editor.CellPainted/Erased/NewLevel/LevelLoaded/TeleporterDirectionChanged` | 否（始终可见） |

新增 overlay 只需继承 `UEditorOverlayComponent`、订阅需要的事件，不改 Manager 也不改 GameMode。

**LevelEditorPawn**：编辑器输入处理、对话框管理。

### 7. 教程系统（Tutorial/）

**TutorialSubsystem**（`UWorldSubsystem`）：
- 在 `OnWorldBeginPlay` 中订阅 GameEventBus 事件
- 收到事件后自动映射到 `ETutorialConditionType` 进行条件匹配
- 支持游玩和编辑器两种上下文，通过 `bIsEditorTutorial` 区分
- 不持有游戏逻辑引用，可完全移除不影响游戏

**条件模型**：触发条件和完成条件共用 `FTutorialCondition` 结构体 + `ETutorialConditionType` 枚举。

**数据配置**：`TutorialDataAsset` 按关卡索引存储教程步骤，在 UE 编辑器中直接编辑。

### 8. UI 层（UI/）

**菜单**：MainMenuWidget, PresetLevelSelectWidget, CustomLevelSelectWidget

**游玩中**：PauseMenuWidget, TutorialWidget, TuiXiangZiPlayerController

**编辑器 UI**：EditorMainWidget, Sidebar, Toolbar, StatusBar, GroupManagerPanel, Dialogs（New/Load/Save/Confirm）, ValidationResultPanel, ColorPickerPopup

所有 UI 组件使用 UMG + `BindWidget` 模式，C++ 定义逻辑、Blueprint 定义布局。

---

## 数据流

### 游玩移动

```
玩家输入 → SokobanCharacter::OnMoveInput
  → GridManager::TryMoveActor
    → 校验：IsCellPassable + CanPushBoxTo + 门阻挡检查
    → 更新占位关系
    → 检查 Modifier：ShouldContinueMovement → 计算滑行终点
    → 广播 OnActorLogicalMoved（GameEventBus）
  → Timeline 动画插值
  → GameState::CaptureSnapshot（撤销栈）
  → IncrementSteps → CheckGoalCondition
```

### 关卡加载

```
SokobanGameMode::LoadLevel(JsonPath)
  → LevelSerializer::LoadFromJson → FLevelData
  → GridManager::InitFromLevelData
    → 创建 FGridCell 网格
    → 生成 TileVisualActor + 挂载 Mechanism/Modifier 组件
    → 应用分组颜色
  → 设置玩家起始位置
```

### 编辑器保存

```
LevelEditorGameMode::SaveLevel
  → 构建 FLevelData（遍历网格 + 收集分组样式）
  → ValidateLevel → 错误阻止保存 / 警告提示
  → LevelSerializer::SaveToJson
```

### 编辑器测试

```
LevelEditorGameMode::TestLevel
  → ValidateLevel
  → 序列化当前网格 → FLevelData
  → 切换 GameMode → SokobanGameMode
  → 加载 FLevelData → 游玩
  → 退出测试 → 恢复编辑器状态
```

---

## 设计模式

| 模式 | 应用 |
|------|------|
| **网格离散化** | 所有位置用 `FIntPoint`，避免浮点对齐问题 |
| **组件化机制** | Mechanism/Modifier 组件挂载到 TileVisualActor，即插即用 |
| **描述表驱动** | `FCellTypeDescriptor` + `FBrushDescriptor`，单行配置贯通全链路 |
| **事件驱动** | GameEventBus（`UWorldSubsystem`）+ `FName` 标签解耦所有子系统；编辑器 overlay 通过事件独立更新，GameMode 无直接引用 |
| **快照撤销** | GameState 维护 `TArray<FLevelSnapshot>`，直接恢复完整状态 |
| **样式目录** | TileStyleCatalog DataAsset 集中管理视觉样式 |
| **GameMode 分离** | 编辑器/游玩各自 GameMode，共享 GridManager 和数据格式 |
| **条件模型复用** | 触发/完成条件共用同一结构体，减少概念数量 |
