# Phase 3: Gameplay Logic — 详细实施计划

## 概述

Phase 3 在 Phase 2 完成后开始，实现核心玩法逻辑。3B（机关系统）和 3C（目标格+冰面）可并行开发；3A（移动逻辑）的 `PostMoveSettlement` 需调用 3B 的 `CheckAllPressurePlateGroups` 和 3C 的 `CheckGoalCondition`，因此 3A 的完整实现需等 3B/3C 就绪。建议先开发 3A 的 TryMoveActor 主体逻辑（不含 PostMoveSettlement），与 3B/3C 并行，最后集成 PostMoveSettlement。

## 3A: GridManager 移动逻辑

### 文件清单
- `Grid/GridManager.h/.cpp` (扩展)
- `Gameplay/SokobanCharacter.h/.cpp` (扩展输入回调)
- `Gameplay/PushableBox.h/.cpp` (扩展事件响应)

### 新增声明

```cpp
// 事件委托
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPitFilled, FIntPoint, AActor*);
FOnPitFilled OnPitFilled;

// 核心接口
bool TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction);

// 内部辅助
bool ExecuteSingleMove(FIntPoint FromGrid, FIntPoint ToGrid, EMoveDirection Direction);
void HandleBoxFallIntoPit(AActor* BoxActor, FIntPoint PitPos);
void UpdateOccupancy(FIntPoint OldPos, FIntPoint NewPos, AActor* Actor);
FIntPoint CalculateIceSlideDestination(FIntPoint StartPos, EMoveDirection Direction);
void PostMoveSettlement();
```

### TryMoveActor 完整流程

```
1. 计算目标格 TargetPos = FromGrid + DirectionToOffset(Dir)
2. 获取源格 OccupyingActor
3. 分支A: 目标格有箱子
   3A-1: 只有玩家能推箱子
   3A-2: 门关闭时门上箱子不可推
   3A-3: 计算箱子前方 BoxTargetPos
   3A-4: CanPushBoxTo(BoxTargetPos) 检查
   3A-5: BoxTargetPos 有占用 → 不可推（不支持连锁）
   分支A-坑洞: HandleBoxFallIntoPit + 玩家前进
   分支A-冰面: 箱子滑行 + 玩家移动（可能也滑行）
   分支A-普通: 箱子前进 + 玩家前进
4. 分支B: 目标格无占用
   4B-1: IsCellPassable 检查
   4B-2: 执行移动
   4B-3: 冰面滑行处理
5. PostMoveSettlement（踏板、门、通关检测）
```

### HandleBoxFallIntoPit（坑洞填平）

```
1. 清除箱子原位置占用
2. SetCell(PitPos, Floor) — 不可逆
3. 广播 OnPitFilled
4. 箱子 Actor 隐藏 + SetLifeSpan(1.0f) 延迟销毁
```

### UpdateOccupancy

```
1. 清空旧位置 OccupyingActor
2. 设置新位置 OccupyingActor
3. 同步 Actor 自身的 CurrentGridPos 缓存
```

---

## 3B: 机关系统

> **注意**: GridManager 的 `SpawnOrUpdateVisualActor` 已跳过 PressurePlate/Door 类型（Phase 1B），机关格子的可视化完全由此处的专用 Actor 负责。

### 文件清单
- `Gameplay/Mechanisms/GridMechanism.h/.cpp` — 抽象基类
- `Gameplay/Mechanisms/PressurePlate.h/.cpp` — 踏板
- `Gameplay/Mechanisms/Door.h/.cpp` — 门
- `Grid/GridManager.h/.cpp` (扩展分组检测)

### AGridMechanism 基类

```cpp
UCLASS(Abstract)
class AGridMechanism : public AActor
{
    FIntPoint GridPos;
    int32 GroupId = -1;
    virtual void OnActivate();
    virtual void OnDeactivate();
    virtual bool IsActivated() const;
    virtual void SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor);
protected:
    bool bIsActivated = false;
    FLinearColor CachedBaseColor, CachedActiveColor;
    UMaterialInstanceDynamic* DynMaterial = nullptr;
};
```

### APressurePlate
- Cylinder Mesh 扁平化，动态材质按 GroupId 着色
- OnActivate: 颜色变亮 + EmissiveStrength 增加
- OnDeactivate: 恢复暗色

### ADoor
- Cube Mesh，动态材质按 GroupId 着色
- Timeline 控制 Z 轴下沉（开门）/ 上升（关门），0.5 秒
- `SetDoorStateImmediate(bool bOpen)` 用于初始化和撤销恢复

### 分组检测核心方法 CheckAllPressurePlateGroups

```
每次移动后调用:
1. 收集所有 GroupId
2. 逐组: AreAllPlatesActiveInGroup(GroupId)
   - 遍历同组所有踏板，检查 OccupyingActor != nullptr
3. 全激活 → SetDoorOpen(true)，任一失活 → SetDoorOpen(false)
4. 更新踏板 Actor 视觉状态
```

### 颜色 Fallback
- 从 `FLevelData::GroupStyles` 按 GroupId 查颜色
- 找不到时用 `FRandomStream(GroupId * 12345)` 生成随机色

---

## 3C: 目标格 + 冰面

### CheckGoalCondition

```
遍历 GoalPositions，检查 OccupyingActor 是否为 ASokobanCharacter
→ 是则广播 OnPlayerEnteredGoal
```

### CalculateIceSlideDestination 核心逻辑

```cpp
FIntPoint CurrentPos = StartPos; // 已确认是 Ice
while (Step < 100):
    NextPos = CurrentPos + Offset
    if !HasCell(NextPos): break          // Empty → 停止
    if NextCell.CellType != Ice: break   // 非冰面 → 停止
    if NextCell.OccupyingActor: break    // 有障碍 → 停止
    if !IsCellPassable(NextPos): break   // 不可通行 → 停止
    CurrentPos = NextPos
return CurrentPos  // 停在最后一格冰面上
```

### 冰面关键规则
- 进入冰面格后立即触发滑行，沿移动方向持续滑动
- 滑行终止条件：下一格不是冰面（Floor/Wall/Goal 等）、下一格有占用 Actor、下一格不存在（Empty）
- 滑行者**停在最后一格冰面上**，不会滑出冰面区域落到非冰面格
- 遇障碍物（墙/占用/不可通行）停在障碍物前一格冰面
- 不进入 Empty（坑洞）
- 不连锁推箱子（冰面上推箱子时，箱子前方有另一个箱子则不可推）
- 玩家在冰面推箱子的完整流程：
  1. 箱子先执行滑行，计算箱子终点 BoxDest
  2. 玩家移动到箱子原位置（现为冰面）
  3. 玩家从该冰面触发滑行，滑行终点受 BoxDest 上箱子的阻挡（停在 BoxDest 前一格）
  4. 若玩家原位置和箱子原位置之间无冰面，则玩家仅前进一格不滑行

### 边界情况处理清单
1. 门关闭时门上有箱子 → 箱子不可推动
2. 坑洞不可逆（撤销系统通过快照恢复）
3. 冰面上推箱子 → 箱子先滑行，玩家后滑行
4. 推箱子时箱子前方也有箱子 → 不可推
5. 玩家自身站踏板 → 踏板激活
6. 初始状态箱子在踏板上 → InitFromLevelData 第8步检测（完整 9 步流程见 Phase4_FrameworkEditor.md）
7. 门关闭瞬间玩家在门上 → 允许离开（不检查 FromGrid 通行性）

### 验收标准
- 3A: 四方向移动、推箱子、坑洞填平、移动事件广播、移动锁
- 3B: 踏板/门生成、颜色配对、分组激活/关闭、多组独立
- 3C: Goal 通关检测、冰面滑行规则全部正确
