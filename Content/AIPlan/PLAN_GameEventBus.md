# GameEventBus 实现计划

## 目标

引入中央事件总线 `UGameEventBus`（`UWorldSubsystem`），统一项目中所有游戏逻辑事件的广播与订阅，替换当前分散在各类上的委托和对 TutorialSubsystem 的直接调用。

### 当前问题

```
SokobanCharacter ──→ TutorialSubsystem (直接调用 NotifyCondition)
PushableBox ──────→ TutorialSubsystem (直接调用 NotifyCondition)
DoorMechanism ────→ TutorialSubsystem (直接调用 NotifyCondition)
SokobanGameMode ──→ TutorialSubsystem (直接调用 Notify/Pause/Dismiss)
LevelEditorGM ────→ TutorialSubsystem (直接调用 NotifyGameplayEvent)
GridManager ──────→ Character/Box (自有委托 OnActorLogicalMoved)
GridManager ──────→ GameMode (自有委托 OnPlayerEnteredGoal)
```

每个通知方都 `#include` 并 `GetSubsystem<UTutorialSubsystem>()`。新增消费系统（成就、数据统计）就要在所有通知点再加一轮调用。

### 目标架构

```
GridManager ────────┐
SokobanCharacter ───┤
PushableBox ────────┤                    ┌→ TutorialSubsystem (订阅)
DoorMechanism ──────┤──→ GameEventBus ───┼→ SokobanCharacter (订阅 ActorMoved)
SokobanGameMode ────┤                    ├→ PushableBoxComponent (订阅 ActorMoved)
LevelEditorGM ──────┘                    ├→ SokobanGameMode (订阅 PlayerEnteredGoal)
                                         └→ 未来: 成就/统计系统 (订阅)
```

### 不迁移的部分

- **UI 控件委托**（工具栏按钮、笔刷选择、对话框确认/取消、分组管理面板）— 父子控件之间的直接通信，走事件总线过度设计
- **Timeline 动画回调** — 内部动画机制，与业务逻辑无关
- **TutorialWidget 的 OnAdvanced 委托** — 控件到 Subsystem 的回调，保持不变

---

## 事件标签命名规范

使用点分层级 `FName` 标签，按域名前缀分组：

| 事件标签 | 替代 | Payload 使用字段 |
|---------|------|-----------------|
| `Grid.ActorMoved` | `GridManager::OnActorLogicalMoved` | Actor, FromPos, ToPos |
| `Grid.PlayerEnteredGoal` | `GridManager::OnPlayerEnteredGoal` | GridPos |
| `Grid.PitFilled` | `GridManager::OnPitFilled` | Actor, GridPos |
| `Game.StepCountChanged` | `SokobanGameMode::OnStepCountChanged` | IntParam (步数) |
| `Game.LevelCompleted` | `SokobanGameMode::OnLevelCompleted` | IntParam (总步数) |
| `Player.Moved` | `TutSub->NotifyCondition(OnPlayerMove)` + `NotifyPlayerMoved` | GridPos |
| `Player.PushedBox` | `TutSub->NotifyCondition(OnPushBox)` | — |
| `Player.Undone` | `TutSub->NotifyCondition(OnUndo)` | — |
| `Player.Reset` | `TutSub->NotifyCondition(OnReset)` | — |
| `Mechanism.DoorOpened` | `TutSub->NotifyCondition(OnDoorOpened)` | — |
| `Editor.BrushChanged` | `NotifyGameplayEvent("BrushChanged")` | — |
| `Editor.CellPainted` | `NotifyGameplayEvent("CellPainted")` | — |
| `Editor.CellErased` | `NotifyGameplayEvent("CellErased")` | — |
| `Editor.GroupCreated` | `NotifyGameplayEvent("GroupCreated")` | — |
| `Editor.ModeChanged` | `NotifyGameplayEvent("ModeChanged")` | — |
| `Editor.NewLevel` | `NotifyGameplayEvent("NewLevel")` | — |
| `Editor.LevelSaved` | `NotifyGameplayEvent("LevelSaved")` | — |
| `Editor.LevelLoaded` | `NotifyGameplayEvent("LevelLoaded")` | — |
| `Editor.LevelTested` | `NotifyGameplayEvent("LevelTested")` | — |

---

## 新增文件

### `Source/TuiXiangZi/Events/GameEventPayload.h`

通用事件载荷结构体：

```cpp
USTRUCT(BlueprintType)
struct FGameEventPayload
{
    GENERATED_BODY()

    TWeakObjectPtr<AActor> Actor;
    FIntPoint GridPos = FIntPoint::ZeroValue;
    FIntPoint FromPos = FIntPoint::ZeroValue;
    FIntPoint ToPos   = FIntPoint::ZeroValue;
    int32 IntParam    = 0;
    bool BoolParam    = false;
    FName NameParam   = NAME_None;

    // 便捷工厂方法（inline 实现，无需额外 .cpp）
    static FGameEventPayload MakeActorMoved(AActor* InActor, FIntPoint From, FIntPoint To)
    {
        FGameEventPayload P;
        P.Actor = InActor; P.FromPos = From; P.ToPos = To;
        return P;
    }
    static FGameEventPayload MakeGridPos(FIntPoint Pos)
    {
        FGameEventPayload P;
        P.GridPos = Pos;
        return P;
    }
    static FGameEventPayload MakeActorAtPos(AActor* InActor, FIntPoint Pos)
    {
        FGameEventPayload P;
        P.Actor = InActor; P.GridPos = Pos;
        return P;
    }
    static FGameEventPayload MakeInt(int32 Value)
    {
        FGameEventPayload P;
        P.IntParam = Value;
        return P;
    }
};
```

### `Source/TuiXiangZi/Events/GameEventTags.h`

事件标签常量集中定义（防拼写错误，便于 grep）：

```cpp
namespace GameEventTags
{
    inline const FName ActorMoved         = TEXT("Grid.ActorMoved");
    inline const FName PlayerEnteredGoal  = TEXT("Grid.PlayerEnteredGoal");
    inline const FName PitFilled          = TEXT("Grid.PitFilled");
    inline const FName StepCountChanged   = TEXT("Game.StepCountChanged");
    inline const FName LevelCompleted     = TEXT("Game.LevelCompleted");
    inline const FName PlayerMoved        = TEXT("Player.Moved");
    inline const FName PushedBox          = TEXT("Player.PushedBox");
    inline const FName Undone             = TEXT("Player.Undone");
    inline const FName Reset              = TEXT("Player.Reset");
    inline const FName DoorOpened         = TEXT("Mechanism.DoorOpened");
    inline const FName EditorBrushChanged = TEXT("Editor.BrushChanged");
    inline const FName EditorCellPainted  = TEXT("Editor.CellPainted");
    inline const FName EditorCellErased   = TEXT("Editor.CellErased");
    inline const FName EditorGroupCreated = TEXT("Editor.GroupCreated");
    inline const FName EditorModeChanged  = TEXT("Editor.ModeChanged");
    inline const FName EditorNewLevel     = TEXT("Editor.NewLevel");
    inline const FName EditorLevelSaved   = TEXT("Editor.LevelSaved");
    inline const FName EditorLevelLoaded  = TEXT("Editor.LevelLoaded");
    inline const FName EditorLevelTested  = TEXT("Editor.LevelTested");
}
```

### `Source/TuiXiangZi/Events/GameEventBus.h` + `.cpp`

核心总线子系统：

```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameEvent, FName, const FGameEventPayload&);

UCLASS()
class UGameEventBus : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void Broadcast(FName EventTag, const FGameEventPayload& Payload = FGameEventPayload());
    FDelegateHandle Subscribe(FName EventTag, const FOnGameEvent::FDelegate& Delegate);
    void Unsubscribe(FName EventTag, FDelegateHandle Handle);
    void UnsubscribeAll(FName EventTag, const void* Object);
    void UnsubscribeAllForObject(const void* Object);
private:
    TMap<FName, FOnGameEvent> EventDelegates;
    // 反向索引：Object → 其所有订阅的 (EventTag, Handle) 列表，使 UnsubscribeAllForObject 为 O(订阅数) 而非 O(事件标签总数)
    TMap<const void*, TArray<TPair<FName, FDelegateHandle>>> ObjectSubscriptions;
};
```

设计决策：
- 使用 native `DECLARE_MULTICAST_DELEGATE`（非 dynamic），所有订阅者都是 C++，性能更好
- `TMap<FName, FOnGameEvent>` 按标签分桶，O(1) 查找
- `ObjectSubscriptions` 反向索引确保 `UnsubscribeAllForObject` 高效退订，`Subscribe` 时同步写入，`Unsubscribe` 时同步移除

---

## 实施阶段

### Phase A: 创建基础设施（不改现有代码）

1. 创建 `Events/GameEventPayload.h`（工厂方法为 inline，无需 `.cpp`）
2. 创建 `Events/GameEventTags.h`
3. 创建 `Events/GameEventBus.h` + `GameEventBus.cpp`

此阶段结束后可编译验证新文件无误。

---

### Phase B: 广播方 + 订阅方迁移（原子步骤）

> **重要**：广播方和订阅方必须在同一阶段完成，否则中间态会导致订阅方（如 TutorialSubsystem）既收不到旧的直接调用，也收不到新的 EventBus 事件，教程系统完全失效。

#### B-1: 广播方迁移

在每个现有广播/直接调用点，改为通过 EventBus 广播。

#### GridManager.cpp

- 添加 `#include "Events/GameEventBus.h"` 和 `#include "Events/GameEventTags.h"`
- `TryMoveActor` 开头缓存指针：`UGameEventBus* EventBus = GetWorld()->GetSubsystem<UGameEventBus>();`
- 约 10 处 `OnActorLogicalMoved.Broadcast(Actor, From, To)` → `EventBus->Broadcast(GameEventTags::ActorMoved, FGameEventPayload::MakeActorMoved(Actor, From, To))`
- `OnPlayerEnteredGoal.Broadcast(GoalPos)` → `EventBus->Broadcast(GameEventTags::PlayerEnteredGoal, FGameEventPayload::MakeGridPos(GoalPos))`
- `OnPitFilled.Broadcast(PitPos, BoxActor)` → `EventBus->Broadcast(GameEventTags::PitFilled, FGameEventPayload::MakeActorAtPos(BoxActor, PitPos))`

#### SokobanCharacter.cpp

- 添加 EventBus 相关 include，删除 `#include "Tutorial/TutorialSubsystem.h"`
- 3 处 `TutSub->NotifyCondition/NotifyPlayerMoved/NotifyStepCountChanged` → `EventBus->Broadcast(PlayerMoved/StepCountChanged)`
- 2 处 `GM->OnStepCountChanged.Broadcast(...)` → `EventBus->Broadcast(GameEventTags::StepCountChanged, ...)`

#### PushableBoxComponent.cpp

- 添加 EventBus 相关 include，删除 `#include "Tutorial/TutorialSubsystem.h"`
- `TutSub->NotifyCondition(OnPushBox)` → `EventBus->Broadcast(GameEventTags::PushedBox)`

#### DoorMechanismComponent.cpp

- 添加 EventBus 相关 include，删除 `#include "Tutorial/TutorialSubsystem.h"`
- `TutSub->NotifyCondition(OnDoorOpened)` → `EventBus->Broadcast(GameEventTags::DoorOpened)`

#### SokobanGameMode.cpp

- 添加 EventBus 相关 include
- `UndoLastMove` 中：`OnStepCountChanged.Broadcast(...)` → `EventBus->Broadcast(StepCountChanged, ...)`；`TutSub->NotifyCondition(OnUndo)` → `EventBus->Broadcast(Undone)`
- `ResetCurrentLevel` 中：`TutSub->NotifyCondition(OnReset)` → `EventBus->Broadcast(Reset)`；`TutSub->DismissTutorial()` 保持直接调用（命令语义，单消费者）
- `OnPlayerEnteredGoal` 中：`OnLevelCompleted.Broadcast(Steps)` → `EventBus->Broadcast(LevelCompleted, ...)`
- `ShowPauseMenu` / `HidePauseMenu` 中：`TutSub->SetPaused(true/false)` 保持直接调用（命令语义，单消费者）

#### LevelEditorGameMode.cpp

- 添加 EventBus 相关 include
- `NotifyEditorTutorialEvent` 方法体改为 `EventBus->Broadcast(EventTag)`
- 各调用点改用 `GameEventTags::Editor*` 常量

#### B-2: 订阅方迁移

#### TutorialSubsystem

- 添加 `virtual void OnWorldBeginPlay(UWorld& InWorld) override;`
- 在 `OnWorldBeginPlay` 中订阅所有关心的事件：
  - `PlayerMoved` → 内部处理 `OnPlayerMove` + `OnGridPosition` 两种条件
  - `PushedBox` → 内部处理 `OnPushBox`
  - `Undone` → 内部处理 `OnUndo`
  - `Reset` → 内部处理 `OnReset`
  - `DoorOpened` → 内部处理 `OnDoorOpened`
  - `StepCountChanged` → 内部处理 `AfterSteps`
  - `Editor.*` 事件 → 作为 `OnGameplayEvent` 条件匹配
- 删除公开方法：`NotifyCondition`、`NotifyGameplayEvent`、`NotifyStepCountChanged`、`NotifyPlayerMoved`
- `SetPaused` 和 `DismissTutorial` 保持 public（SokobanGameMode 仍直接调用）

#### SokobanCharacter

- `BeginPlay` 中：`GridManagerRef->OnActorLogicalMoved.AddUObject(this, ...)` → `EventBus->Subscribe(GameEventTags::ActorMoved, ...)`
- 重命名 `OnActorLogicalMoved(AActor*, FIntPoint, FIntPoint)` → `OnActorMovedEvent(FName Tag, const FGameEventPayload& Payload)`，内部用 `Payload.Actor`/`Payload.ToPos`

#### PushableBoxComponent

- `BeginPlay` 中：同上替换订阅
- `EndPlay` 中：`GridManagerRef->OnActorLogicalMoved.RemoveAll(this)` → `EventBus->UnsubscribeAll(GameEventTags::ActorMoved, this)`
- 重命名 handler 签名

#### SokobanGameMode

- `BeginPlay` 中：`GridManagerRef->OnPlayerEnteredGoal.AddUObject(this, ...)` → `EventBus->Subscribe(GameEventTags::PlayerEnteredGoal, ...)`
- 保留 `OnStepCountChanged` 和 `OnLevelCompleted` 作为 `BlueprintAssignable` 包装器：
  ```cpp
  EventBus->Subscribe(GameEventTags::StepCountChanged, Lambda → OnStepCountChanged.Broadcast(Payload.IntParam));
  EventBus->Subscribe(GameEventTags::LevelCompleted, Lambda → OnLevelCompleted.Broadcast(Payload.IntParam));
  ```
- 重命名 `OnPlayerEnteredGoal(FIntPoint)` → `OnPlayerEnteredGoalEvent(FName, const FGameEventPayload&)`

---

---

### Phase C: 清理

#### GridManager.h

- 删除 3 个 `DECLARE_MULTICAST_DELEGATE` 声明
- 删除 3 个委托成员变量 (`OnActorLogicalMoved`, `OnPlayerEnteredGoal`, `OnPitFilled`)

#### 头文件更新

- `SokobanCharacter.h`：handler 签名改为 `void OnActorMovedEvent(FName, const FGameEventPayload&)`
- `PushableBoxComponent.h`：同上
- `SokobanGameMode.h`：handler 签名更新；添加 EventBus subscription handle 成员

#### Include 清理

从以下文件中删除 `#include "Tutorial/TutorialSubsystem.h"`（不再直接调用）：
- `SokobanCharacter.cpp`
- `PushableBoxComponent.cpp`
- `DoorMechanismComponent.cpp`
- `LevelEditorGameMode.cpp`（保留——仍需 `SetTutorialConfig`/`StartEditorTutorial`）
- `SokobanGameMode.cpp`（保留——仍需 `SetTutorialConfig`/`StartTutorial`/`SetPaused`/`DismissTutorial`）

#### ARCHITECTURE.md

- 新增 Events 子系统章节
- 更新 GridManager、Tutorial 等章节的描述

---

## 广播站点迁移对照表

| 文件 | 当前代码 | 新代码 |
|------|---------|--------|
| GridManager.cpp (×10) | `OnActorLogicalMoved.Broadcast(Actor, From, To)` | `EventBus->Broadcast(ActorMoved, MakeActorMoved(...))` |
| GridManager.cpp | `OnPlayerEnteredGoal.Broadcast(GoalPos)` | `EventBus->Broadcast(PlayerEnteredGoal, MakeGridPos(...))` |
| GridManager.cpp | `OnPitFilled.Broadcast(PitPos, BoxActor)` | `EventBus->Broadcast(PitFilled, ...)` |
| SokobanCharacter.cpp (×2) | `GM->OnStepCountChanged.Broadcast(...)` | `EventBus->Broadcast(StepCountChanged, MakeInt(...))` |
| SokobanCharacter.cpp (×3) | `TutSub->NotifyCondition/PlayerMoved/StepCount` | `EventBus->Broadcast(PlayerMoved/StepCountChanged)` |
| PushableBoxComponent.cpp | `TutSub->NotifyCondition(OnPushBox)` | `EventBus->Broadcast(PushedBox)` |
| DoorMechanismComponent.cpp | `TutSub->NotifyCondition(OnDoorOpened)` | `EventBus->Broadcast(DoorOpened)` |
| SokobanGameMode.cpp | `TutSub->NotifyCondition(OnReset)` + `DismissTutorial` | `EventBus->Broadcast(Reset)` + `DismissTutorial` 保持直接调用 |
| SokobanGameMode.cpp | `TutSub->NotifyCondition(OnUndo)` | `EventBus->Broadcast(Undone)` |
| SokobanGameMode.cpp | `OnStepCountChanged.Broadcast(...)` | `EventBus->Broadcast(StepCountChanged, ...)` |
| SokobanGameMode.cpp | `OnLevelCompleted.Broadcast(Steps)` | `EventBus->Broadcast(LevelCompleted, MakeInt(...))` |
| SokobanGameMode.cpp | `TutSub->SetPaused(true/false)` | 保持直接调用（命令语义，单消费者） |
| LevelEditorGameMode.cpp (×9) | `TutSub->NotifyGameplayEvent(Tag)` | `EventBus->Broadcast(Editor.*)` |

## 订阅站点迁移对照表

| 文件 | 当前订阅 | 新订阅 |
|------|---------|--------|
| SokobanCharacter.cpp | `GridManager->OnActorLogicalMoved.AddUObject` | `EventBus->Subscribe(ActorMoved, ...)` |
| PushableBoxComponent.cpp | `GridManager->OnActorLogicalMoved.AddUObject` | `EventBus->Subscribe(ActorMoved, ...)` |
| PushableBoxComponent.cpp (EndPlay) | `GridManager->OnActorLogicalMoved.RemoveAll` | `EventBus->UnsubscribeAll(ActorMoved, this)` |
| SokobanGameMode.cpp | `GridManager->OnPlayerEnteredGoal.AddUObject` | `EventBus->Subscribe(PlayerEnteredGoal, ...)` |
| TutorialSubsystem (隐式) | 外部调用 `Notify*()` 方法推送事件 | 自行在 `OnWorldBeginPlay` 中订阅 EventBus |

---

## 风险点与注意事项

### 1. 同步派发（关键）

`TryMoveActor` 中箱子和玩家的 `ActorMoved` 有严格先后顺序依赖（动画链）。EventBus 必须同步广播。使用 native `DECLARE_MULTICAST_DELEGATE` 天然同步，无问题。**绝不能改为异步/队列模式。**

### 2. Blueprint 兼容性

`OnStepCountChanged` 和 `OnLevelCompleted` 是 `BlueprintAssignable` 动态多播委托，被 Blueprint UI 控件绑定。不能直接删除。GameMode 保留这两个委托作为包装器，内部订阅 EventBus 后转发给 Blueprint。

### 3. StepCountChanged 重复广播

当前 Undo 操作时，`SokobanCharacter::OnUndo` 和 `SokobanGameMode::UndoLastMove` 各广播一次 `OnStepCountChanged`（疑似 bug）。迁移时合并为单次广播（只在 `UndoLastMove` 中广播）。

### 4. Subsystem 初始化顺序

`UWorldSubsystem::OnWorldBeginPlay` 在所有 Actor 的 `BeginPlay` 之后调用。`SokobanGameMode::BeginPlay` 中调用 `StartTutorial`，此时 TutorialSubsystem 的 EventBus 订阅尚未建立。但这没问题——`StartTutorial` 只设置初始状态和显示第一步，EventBus 订阅用于后续运行时通知。若需更安全，可改在 `Initialize()` 中订阅。

### 5. 对象生命周期

Character/Box 销毁时必须显式 Unsubscribe。`CreateUObject` 绑定的委托在 UObject GC 后自动失效，但显式退订更安全，避免 EventBus 向已销毁对象广播。在 `EndPlay` 中调用 `EventBus->UnsubscribeAllForObject(this)`。

### 6. Editor vs Gameplay 隔离

EventBus 是 `UWorldSubsystem`，每个 World 各自实例化。编辑器和游玩使用不同 World，事件天然隔离，无需额外处理。

### 7. PlayerMoved 合并两个通知

当前 `SokobanCharacter::OnMoveInput` 同时调用 `NotifyCondition(OnPlayerMove)` 和 `NotifyPlayerMoved(GridPos)`，分别用于 `OnPlayerMove`（动作匹配）和 `OnGridPosition`（位置匹配）两种教程条件。迁移后统一为一个 `Player.Moved` 事件（携带 GridPos），TutorialSubsystem handler 中同时尝试两种条件匹配。

### 8. 保留的直接调用

以下调用不走 EventBus，保持直接调用：
- `SokobanGameMode::ExecuteLoadLevel` 中的 `TutSub->SetTutorialConfig()` + `TutSub->StartTutorial()` — 初始化配置，非运行时事件
- `LevelEditorGameMode::BeginPlay` 中的 `TutSub->SetTutorialConfig()` + `TutSub->StartEditorTutorial()` — 同上
- `TutorialWidget::OnAdvanced` → `TutorialSubsystem::AdvanceTutorial()` — 控件回调，保持不变
- `SokobanGameMode::ShowPauseMenu/HidePauseMenu` 中的 `TutSub->SetPaused(true/false)` — 命令语义，单消费者，走 EventBus 增加间接层无解耦收益
- `SokobanGameMode::ResetCurrentLevel` 中的 `TutSub->DismissTutorial()` — 同上

---

## 文件总览

### 新增文件（4 个）
- `Source/TuiXiangZi/Events/GameEventPayload.h`
- `Source/TuiXiangZi/Events/GameEventTags.h`
- `Source/TuiXiangZi/Events/GameEventBus.h`
- `Source/TuiXiangZi/Events/GameEventBus.cpp`

### 修改文件（12 个）
- `Source/TuiXiangZi/Grid/GridManager.h` — 删除 3 个委托声明和成员
- `Source/TuiXiangZi/Grid/GridManager.cpp` — 替换所有 Broadcast 调用
- `Source/TuiXiangZi/Framework/SokobanGameMode.h` — 添加 EventBus handle 成员，更新 handler 签名
- `Source/TuiXiangZi/Framework/SokobanGameMode.cpp` — 替换广播和订阅
- `Source/TuiXiangZi/Gameplay/SokobanCharacter.h` — 更新 handler 签名
- `Source/TuiXiangZi/Gameplay/SokobanCharacter.cpp` — 替换订阅和广播
- `Source/TuiXiangZi/Gameplay/PushableBoxComponent.h` — 更新 handler 签名
- `Source/TuiXiangZi/Gameplay/PushableBoxComponent.cpp` — 替换订阅和广播
- `Source/TuiXiangZi/Gameplay/Mechanisms/DoorMechanismComponent.cpp` — 替换广播
- `Source/TuiXiangZi/Editor/LevelEditorGameMode.cpp` — 替换广播
- `Source/TuiXiangZi/Tutorial/TutorialSubsystem.h` — 删除公开 Notify 方法，添加 EventBus handler
- `Source/TuiXiangZi/Tutorial/TutorialSubsystem.cpp` — 添加 EventBus 订阅，内化通知逻辑
- `ARCHITECTURE.md` — 新增 Events 子系统文档
