# Phase 4: Framework & Editor — 详细实施计划

## 概述

Phase 4 依赖 Phase 3，包含两个可并行子任务。

## 4A: SokobanGameMode + GameState + 撤销系统

### 文件清单
```
Source/TuiXiangZi/Framework/
├── SokobanGameMode.h/.cpp
└── SokobanGameState.h/.cpp

需修改的已有文件:
├── Gameplay/SokobanCharacter.h/.cpp   — OnMoveInput 中插入快照压入/回滚逻辑
└── Grid/GridManager.h/.cpp            — InitFromLevelData 扩展为完整 9 步流程
```

### ASokobanGameMode 核心接口
- `LoadLevel(FString JsonFilePath)` — 加载关卡 JSON
- `LoadNextLevel()` — 下一关
- `ResetCurrentLevel()` — 重置
- `UndoLastMove()` — 撤销
- `OnPlayerEnteredGoal(FIntPoint)` — 通关回调
- `ReturnToEditor()` / `ReturnToMainMenu()` — 模式切换
- 配置: `TArray<FString> LevelJsonPaths`, `int32 CurrentLevelIndex`
- 编辑器测试: 临时 JSON 路径 `Saved/Levels/_temp_editor_test.json`

### ASokobanGameState 核心接口

```cpp
// 快照结构
struct FDoorSnapshot { FIntPoint DoorPos; bool bDoorOpen; };
struct FPitSnapshot { FIntPoint PitPos; bool bFilled; };
struct FLevelSnapshot {
    FIntPoint PlayerPos;
    TArray<FIntPoint> BoxPositions;
    TArray<FDoorSnapshot> DoorStates;
    TArray<FPitSnapshot> PitStates;
    int32 StepCount;
};

// GameState
class ASokobanGameState : public AGameStateBase
{
    int32 StepCount;
    bool bLevelCompleted;

    FLevelSnapshot CaptureSnapshot(const AGridManager* GM) const;
    void PushSnapshot(const FLevelSnapshot& Snapshot);
    FLevelSnapshot PopSnapshot();
    bool CanUndo() const;
    void RestoreSnapshot(const FLevelSnapshot& Snapshot, AGridManager* GM);
    void ResetState();
};
```

### RestoreSnapshot 7 步重建流程

```
1. 清空所有格子 OccupyingActor（不清 CellType 等静态数据）
2. 恢复坑洞状态：bFilled==false 且当前为 Floor → RemoveCell 恢复 Empty
3. 销毁当前所有箱子 Actor
4. 根据快照 BoxPositions 重新生成箱子 + 写入占用表
5. 恢复玩家位置（Snap 无动画）+ 写入占用表
6. 恢复门的 bDoorOpen 状态（直接从快照读，不重新计算）
7. 刷新所有踏板 Actor 视觉状态
+ 恢复步数
```

关键设计：全量重建箱子（而非逐个移动），门状态从快照直接读（避免中间态），撤销用 Snap 无动画。

### 快照压入时机

```
WASD 按下 → OnMoveInput
  → PushSnapshot(CaptureSnapshot())   // 先压快照
  → TryMoveActor()
  → 成功: IncrementSteps
  → 失败: PopSnapshot()               // 回滚
```

### InitFromLevelData 完整 9 步流程

此流程在 `ASokobanGameMode::LoadLevel` 中调用，整合 Phase 1/3/4 的逻辑：

```
1. ClearGrid() — 清空网格数据和所有可视化 Actor
2. 遍历 FLevelData::Cells，逐格 SetCell — 写入 GridCells TMap + 生成地面/墙壁/冰面/Goal 可视化 Actor
   （PressurePlate/Door 类型跳过基础可视化，由步骤 5 的专用 Actor 负责）
3. 记录 GoalPositions — 从 Cells 中筛选 CellType==Goal 的坐标
4. 生成箱子 — 遍历 FLevelData::BoxPositions，Spawn APushableBox + 写入对应格子 OccupyingActor
5. 生成机关 Actor — 遍历 Cells 中 PressurePlate/Door 类型：
   - PressurePlate → Spawn APressurePlate，设置 GroupId/GridPos
   - Door → Spawn ADoor，设置 GroupId/GridPos，初始 bDoorOpen=false
6. 应用分组颜色 — 从 FLevelData::GroupStyles 按 GroupId 查颜色，设置踏板/门的动态材质
   找不到时用 FRandomStream(GroupId * 12345) 生成随机色
7. 生成玩家 — 在 FLevelData::PlayerStart 位置 Spawn/移动 SokobanCharacter + 写入 OccupyingActor
8. 初始踏板检测 — 调用 CheckAllPressurePlateGroups()，处理初始状态箱子已在踏板上的情况
9. 初始通关检测 — 调用 CheckGoalCondition()，处理起点即为目标格的边界情况
```

---

## 4B: 关卡编辑器核心

### 文件清单
```
Source/TuiXiangZi/Editor/
├── LevelEditorGameMode.h/.cpp
├── LevelEditorPawn.h/.cpp
├── EditorBrushTypes.h
├── EditorGridVisualizer.h/.cpp
```

### 笔刷类型

```cpp
enum class EEditorBrush : uint8
{ Floor, Wall, Ice, Goal, Door, BoxSpawn, PlayerStart, Eraser };

enum class EEditorMode : uint8
{ Normal, PlacingPlates };
```

### ALevelEditorGameMode 核心接口
- `SetCurrentBrush(EEditorBrush)` / `GetCurrentBrush()`
- `PaintAtGrid(FIntPoint)` — 绘制分发
- `EraseAtGrid(FIntPoint)` — 擦除
- `NewLevel(int32 W, int32 H)` / `SaveLevel` / `LoadLevel` / `TestCurrentLevel`
- `CreateNewGroup` / `DeleteGroup` — 分组管理
- 委托: `OnBrushChanged`, `OnEditorModeChanged`

### ALevelEditorPawn 核心接口
- 正交俯视相机 (`ECameraProjectionMode::Orthographic`)
- 滚轮缩放 (`OrthoWidth` 256-4096)
- 中键拖拽平移
- 左键绘制/右键擦除
- `RaycastToGrid(FIntPoint&)` — 射线检测

### 射线检测核心流程

```
鼠标屏幕坐标
  → DeprojectScreenPositionToWorld → 世界射线
  → 与 Z=0 平面求交: T = -Origin.Z / Direction.Z
  → HitPoint = Origin + T * Direction
  → GridManager->WorldToGrid(HitPoint) → 格子坐标
```

### 模式切换机制（OpenLevel + 临时 JSON）
- 编辑器 "测试" → 序列化当前状态为临时 JSON → `OpenLevel("GameMap", "?FromEditor=1&LevelJson=...")`
- 游戏 "返回编辑器" → `OpenLevel("EditorMap")` → 从临时 JSON 恢复
- GameMode 通过 `ParseOption` 解析 Options

### EditorGridVisualizer — 辅助网格渲染
- ProceduralMeshComponent 动态生成网格线
- 范围 = GridBounds 外扩 PaddingCells(2) 格
- 边界变化时重建，未变化时跳过

### UMG Widget 清单
| Widget | 说明 |
|--------|------|
| WBP_GameHUD | 步数、撤销、重置 |
| WBP_LevelComplete | 通关弹窗 |
| WBP_EditorSidebar | 笔刷选择 |
| WBP_EditorToolbar | 新建/保存/加载/测试 |
| WBP_NewLevelDialog | 宽高输入 |
| WBP_ConfirmDialog | 通用确认 |

### Map 资源清单
| Map | GameMode | 说明 |
|-----|----------|------|
| GameMap | ASokobanGameMode | 游戏运行 |
| EditorMap | ALevelEditorGameMode | 关卡编辑 |
| MainMenuMap | 默认 | 主菜单 |

### Build.cs 新增依赖
- `ProceduralMeshComponent` 模块（辅助网格用）

### 验收标准
- 4A: 关卡加载→游玩→步数统计→撤销→通关→下一关全流程
- 4B: 编辑器→笔刷绘制→擦除→扩展/缩减地图→保存→加载→测试→返回全流程
