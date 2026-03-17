# 推箱子解谜游戏 Demo — 分阶段实施计划

## 依赖关系分析

```
Phase 1: Foundation (1A 可独立；1B.cpp 的 InitFromLevelData 依赖 1C)
  ├── 1A: 项目脚手架 + GridTypes 基础数据定义
  ├── 1B: GridManager 核心 (数据存储、坐标转换、格子查询)
  └── 1C: LevelData 数据类型 + LevelSerializer

Phase 2: Core Actors (Phase 1 完成后，全部可并行)
  ├── 2A: SokobanCharacter (格子移动 + 平滑视觉移动)
  ├── 2B: PushableBox (Actor + 平滑移动)
  └── 2C: TileStyleCatalog (DataAsset 视觉样式系统)

Phase 3: Gameplay Logic (Phase 2 完成后；3B/3C 可并行，3A 的 PostMoveSettlement 依赖 3B/3C)
  ├── 3A: GridManager 移动逻辑 (TryMoveActor、推箱子判定、坑洞填平)
  ├── 3B: 机关系统 (基类 + 踏板 + 门 + 分组配对)
  └── 3C: 目标格通关 + 冰面滑行机制

Phase 4: Framework & Editor (Phase 3 完成后，可并行)
  ├── 4A: SokobanGameMode + GameState + 撤销系统
  └── 4B: 关卡编辑器核心 (GameMode、Pawn、基础笔刷编辑)

Phase 5: Polish & Content (Phase 4 完成后，全部可并行)
  ├── 5A: 编辑器高级功能 (分组管理UI、门/踏板关联放置、样式选择器)
  ├── 5B: 关卡设计 (4 个关卡 JSON)
  └── 5C: 视觉反馈 + 集成测试 + 打磨
```

## 阶段间依赖说明

- **Phase 1 内部**: 1A 可独立；1B 头文件可用前向声明独立编译，但 1B.cpp 的 `InitFromLevelData` 实现依赖 1C 的 `LevelDataTypes.h`
- **Phase 1 → Phase 2**: Actor 需要引用 GridTypes 和 GridManager 接口
- **Phase 2 → Phase 3**: 移动逻辑需要操作 Character 和 Box Actor
- **Phase 3 内部**: 3B/3C 可并行；3A 的 `PostMoveSettlement` 调用 3B 的 `CheckAllPressurePlateGroups` 和 3C 的 `CheckGoalCondition`，需等 3B/3C 就绪
- **Phase 3 → Phase 4**: GameMode 需要监听通关事件，编辑器需要所有格子类型就绪
- **Phase 4 → Phase 5**: 高级编辑器功能建立在核心编辑器之上，关卡设计需要完整的序列化管线

---

## Phase 1: Foundation — 基础设施层

> 详细计划见: [Phase1_Foundation.md](Phase1_Foundation.md)

### 1A: 项目脚手架 + GridTypes
- 创建 C++ 模块目录结构
- 实现 GridTypes.h (EGridCellType, FGridCell, EMoveDirection)
- 配置 Build.cs，添加 JsonUtilities 依赖

### 1B: GridManager 核心
- AGridManager Actor 基础框架
- TMap<FIntPoint, FGridCell> 稀疏网格存储
- GridToWorld / WorldToGrid 坐标转换
- GetCell / HasCell / IsCellPassable / CanPushBoxTo 查询接口
- SetCell / RemoveCell 格子增删
- GetGridBounds 动态边界计算
- InitEmptyGrid / ClearGrid
- 基础地面/墙壁可视化 Actor 生成

### 1C: LevelData 数据类型 + Serializer
- FLevelData / FCellData / FMechanismGroupStyleData 数据结构
- ULevelSerializer: SaveToJson / LoadFromJson
- JSON 格式的字符串编码 CellType 映射
- 存储路径管理

---

## Phase 2: Core Actors — 核心 Actor 层

> 详细计划见: [Phase2_CoreActors.md](Phase2_CoreActors.md)

### 2A: SokobanCharacter
- 继承 ACharacter，配置胶囊碰撞体
- Enhanced Input 绑定 WASD 四方向
- 格子化离散移动（输入锁、FTimeline 平滑插值）
- GridManager 引用缓存和事件监听

### 2B: PushableBox
- 简单 AActor + StaticMeshComponent (Cube)
- CurrentGridPos 缓存
- SmoothMoveTo / OnMoveCompleted 平滑移动

### 2C: TileStyleCatalog
- FTileVisualStyle 结构体
- UTileStyleCatalog DataAsset
- FindStyle / GetStylesForType 查询接口
- GridManager 生成 Actor 时的样式查找集成

---

## Phase 3: Gameplay Logic — 玩法逻辑层

> 详细计划见: [Phase3_GameplayLogic.md](Phase3_GameplayLogic.md)

### 3A: GridManager 移动逻辑
- TryMoveActor 完整实现
- 推箱子判定（目标格有箱子→检查箱子前方）
- 坑洞填平机制（箱子推入 Empty→销毁箱子→改为 Floor）
- 移动事件广播 (FOnActorLogicalMoved)
- 角色/箱子 Actor 对移动事件的响应

### 3B: 机关系统
- AGridMechanism 抽象基类
- APressurePlate (表现Actor，GroupId，颜色动态材质)
- ADoor (表现Actor，GroupId，开关动画)
- 分组配对检测逻辑（每次移动后遍历踏板→按组开关门）
- FOnGroupPlatesChanged 委托
- InitFromLevelData 中自动生成踏板/门 Actor 的流程

### 3C: 目标格 + 冰面
- Goal 格视觉指示（发光圈/地贴）
- 通关检测 (OnPlayerEnteredGoal)
- Ice 冰面滑行规则实现
- 滑行边界检测 + 障碍物阻挡
- 冰面上推箱子的特殊处理

---

## Phase 4: Framework & Editor — 框架与编辑器层

> 详细计划见: [Phase4_FrameworkEditor.md](Phase4_FrameworkEditor.md)

### 4A: Game Framework
- ASokobanGameMode (关卡加载、流程控制、重置)
- ASokobanGameState (步数、撤销栈、通关状态)
- FLevelSnapshot 快照结构 (含 DoorSnapshot、PitSnapshot)
- RestoreSnapshot 完整状态重建流程
- InitFromLevelData 完整流程 (9步)

### 4B: Level Editor 核心
- ALevelEditorGameMode (独立GameMode)
- ALevelEditorPawn (俯视正交相机，缩放/平移)
- 模式切换机制 (OpenLevel)
- UMG 侧边栏基础框架（笔刷选择按钮）
- 左键绘制/右键擦除核心交互
- 射线检测 → WorldToGrid → SetCell/RemoveCell
- 辅助网格线渲染

---

## Phase 5: Polish & Content — 打磨与内容层

> 详细计划见: [Phase5_PolishContent.md](Phase5_PolishContent.md)

### 5A: 编辑器高级功能
- 分组管理面板 UI (新建/编辑/删除分组，拾色器)
- 门与踏板关联放置流程（放置门→自动进入踏板模式）
- 样式选择面板（缩略图+名称，从 TileStyleCatalog 加载）
- 起点/目标格唯一性约束
- 安全检查（擦除确认、连通性警告）
- 新建关卡对话框
- 保存/加载/测试按钮

### 5B: 关卡设计
- Level 1: 教学关（1箱子+1踏板+1门+1目标格）
- Level 2: 双组门（2箱子+2组各1踏板1门+1目标格）
- Level 3: 多开关联动（3箱子+1组2踏板+冰面+1目标格）
- Level 4: 综合挑战（4+箱子+3组门+混合地形+目标格）

### 5C: 视觉反馈 + 集成测试
- 箱子到位反馈（动态材质变色）
- 踏板激活发光效果
- 门开关 Timeline 动画
- 冰面滑行视觉效果
- 过关 UI 面板
- 全关卡流程测试
- 撤销系统边界情况测试
- 编辑器功能完整性测试
