# 推箱子解谜游戏 Demo — 技术架构文档

## 项目概述

- **引擎**: Unreal Engine 5.5.4
- **模板**: Top Down Template
- **语言**: C++ 为主，蓝图仅用于资源配置和快速原型
- **视角**: 俯视角（正交/45度）

---

## 核心设计原则

1. **网格驱动（Grid-Based）**: 所有游戏逻辑基于离散格子坐标，不依赖物理碰撞做玩法判定
2. **数据驱动**: 关卡由 JSON 描述，运行时加载和序列化
3. **复用引擎能力**: Character、Enhanced Input、UMG、Timeline 等直接使用，不重复造轮子
4. **逻辑与表现分离**: Grid 层做逻辑判定，Actor 层只负责视觉移动和反馈
5. **单一真相源（Single Source of Truth）**: `GridManager` 是唯一玩法状态源，角色/箱子/门 Actor 只消费状态变化做表现，不反写玩法状态

---

## 模块架构

```
SokobanDemo/
├── Grid/                    # 网格系统（核心）
│   ├── GridManager.h/cpp
│   └── GridTypes.h
├── Gameplay/                # 玩法逻辑
│   ├── SokobanCharacter.h/cpp
│   ├── PushableBox.h/cpp
│   └── Mechanisms/
│       ├── GridMechanism.h/cpp      # 机关基类
│       ├── PressurePlate.h/cpp      # 踏板
│       ├── Door.h/cpp               # 门
│       └── IceTile.h/cpp            # 冰面（扩展机关）
├── LevelData/               # 关卡数据
│   ├── LevelDataTypes.h
│   └── LevelSerializer.h/cpp
├── Editor/                  # Runtime 关卡编辑器
│   ├── LevelEditorGameMode.h/cpp
│   └── LevelEditorPawn.h/cpp
└── Framework/               # 游戏框架
    ├── SokobanGameMode.h/cpp
    └── SokobanGameState.h/cpp
```

---

## 一、Grid 系统

### GridTypes.h — 基础数据定义

```cpp
UENUM(BlueprintType)
enum class EGridCellType : uint8
{
    Empty,          // 坑洞/虚空，玩家不可通行；箱子可被推入坑洞"填平"（变为 Floor）；边界外也按 Empty 处理
    Floor,          // 普通地板
    Wall,           // 墙壁
    PressurePlate,  // 踏板（可通行，由 GroupId 决定配对）；Actor 由 GridManager 根据 CellType 自动生成，仅负责表现
    Ice,            // 冰面
    Goal,           // 目标格，玩家到达时通关
    Door,           // 门（静态地块类型，开关状态由 FGridCell::bDoorOpen 动态控制）
    OneWayGate,     // 单向门（扩展备选）
    Teleporter,     // 传送阵（扩展备选）
};

USTRUCT(BlueprintType)
struct FGridCell
{
    GENERATED_BODY()

    EGridCellType CellType = EGridCellType::Empty;
    TWeakObjectPtr<AActor> OccupyingActor = nullptr; // 当前格子上的可移动实体（箱子/角色），只由 GridManager 写入；踏板/门等机关 Actor 不占此字段
    bool bDoorOpen = false;             // 仅 Door 类型有效：门的动态开关状态（CellType 保持为 Door 不变）
    int32 GroupId = -1;                 // 机关分组编号（仅踏板/门有效，-1 表示无分组）
    int32 ExtraParam = 0;               // 扩展参数（单向门方向、传送配对ID等）
    FName VisualStyleId = NAME_None;    // 地面/墙壁视觉样式引用，NAME_None 使用默认样式
};

// 四方向枚举
UENUM()
enum class EMoveDirection : uint8 { Up, Down, Left, Right };
```

### AGridManager — 核心职责

| 职责 | 说明 |
|------|------|
| 维护网格数据 | `TMap<FIntPoint, FGridCell>` 稀疏网格，仅存储有效格子（Floor/Wall/机关等），不存在于 Map 中的坐标视为 Empty |
| 作为唯一玩法状态源 | 玩家位置、箱子占用、门开关、踏板激活、目标检测都以 Grid 数据为准 |
| 动态边界计算 | 遍历所有已存在的格子坐标，实时计算 AABB 包围盒（`GetGridBounds()`），无固定 Width/Height |
| 坐标转换 | `GridToWorld(FIntPoint)` / `WorldToGrid(FVector)` |
| 格子增删 | `SetCell(FIntPoint, FGridCell)` 添加/修改格子；`RemoveCell(FIntPoint)` 删除格子（变为 Empty） |
| 移动请求处理 | 计算一次输入最终导致的逻辑结果，并写回 Grid 状态 |
| 推箱子判定 | 目标格有箱子 → 检查箱子前方是否可通行或为 Empty（坑洞）→ 可通行则执行连锁移动；为 Empty 则箱子落入坑洞填平（见下方坑洞填平机制） |
| 坑洞填平 | 箱子被推入 Empty 格时：销毁箱子 Actor，将该格 CellType 改为 Floor（变为可通行），播放填坑视觉效果；**填平后的格子不可恢复**，这是一个不可逆操作（撤销系统需记录） |
| 踏板检测 | 每次逻辑移动结算后遍历踏板格，判断是否被箱子**或玩家**占据；离开踏板时同步更新激活状态 |
| 按分组检测 | 按 GroupId 分组检查踏板激活状态，同组踏板全部激活 → 开启同组门；任一踏板失活 → 重新关闭同组门 |
| 门通行判定 | `IsCellPassable()` 对 Door 类型格子检查 `bDoorOpen`：开门可通行，关门不可通行；**门关闭时如果门格子上有箱子，该箱子视为被卡住不可推动** |
| 通关检测 | 玩家进入 `Goal` 格时触发通关事件；Goal 位置在 `InitFromLevelData` 时从 Cells 中扫描 CellType==Goal 的格子得到，不额外存储 |

**关键接口：**

```cpp
// 尝试沿方向移动；成功时先提交逻辑状态，再广播表现层移动命令
bool TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction);

// 查询格子（不存在于 Map 中的坐标返回 Empty 默认值）
const FGridCell& GetCell(FIntPoint GridPos) const;
bool HasCell(FIntPoint GridPos) const;        // 该坐标是否存在有效格子
bool IsCellPassable(FIntPoint GridPos) const; // 存在且可通行（Door 类型额外检查 bDoorOpen）；Empty 对玩家不可通行
bool CanPushBoxTo(FIntPoint GridPos) const;  // 箱子可被推到的位置：IsCellPassable() || CellType==Empty（坑洞可填平）

// 格子增删（编辑器用）
void SetCell(FIntPoint GridPos, const FGridCell& Cell);   // 添加或修改格子
void RemoveCell(FIntPoint GridPos);                        // 删除格子（变为 Empty/虚空）

// 动态边界
FIntRect GetGridBounds() const;  // 遍历所有格子坐标，返回最小 AABB 包围盒

// 坐标转换
FVector GridToWorld(FIntPoint GridPos) const;
FIntPoint WorldToGrid(FVector WorldPos) const;

// 关卡管理
void InitFromLevelData(const FLevelData& Data);
void InitEmptyGrid(int32 Width, int32 Height);  // 编辑器：生成指定宽高的纯 Floor 矩形作为初始画布（不代表网格有固定尺寸，后续可通过笔刷自由扩展/缩减）
void ClearGrid();

/*
 * InitFromLevelData 流程：
 * 1. ClearGrid() 清空旧数据，销毁残留 Actor
 * 2. 设置 GridOrigin
 * 3. 遍历 Cells 列表，将每个格子写入 TMap<FIntPoint, FGridCell>
 * 4. 根据 BoxPositions 生成 APushableBox Actor，写入对应格子的 OccupyingActor
 * 5. 遍历 CellType==PressurePlate 的格子，自动生成 APressurePlate Actor（仅负责表现），设置 GroupId，从 GroupStyles 查颜色
 * 6. 遍历 CellType==Door 的格子，自动生成 ADoor Actor（仅负责表现），设置 GroupId，从 GroupStyles 查颜色
 * 7. 设置玩家起始格 PlayerStart，将玩家 Actor 写入对应格子的 OccupyingActor
 * 8. 扫描 CellType==Goal 的格子，缓存 GoalPositions 列表，生成目标格视觉指示（发光圈/地贴）
 * 9. 执行一次踏板/门状态初始检测（处理箱子初始就在踏板上的情况）
 *
 * InitEmptyGrid 流程（编辑器新建关卡）：
 * 1. ClearGrid()
 * 2. 以 (0,0) 为左下角，填充 Width×Height 的 Floor 格子到 TMap
 * 3. 生成对应的地面可视化 Actor
 */

// 广播给角色/箱子/门 Actor，仅用于表现同步
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActorLogicalMoved, AActor* /*Actor*/, FIntPoint /*From*/, FIntPoint /*To*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerEnteredGoal, FIntPoint /*GoalPos*/);
```

**网格配置参数：**

```cpp
float CellSize = 100.0f;    // 每格世界单位大小
FVector GridOrigin;          // 网格原点世界坐标（坐标转换基准点）
TMap<FIntPoint, FGridCell> GridCells;  // 稀疏网格数据，无固定宽高
TArray<FIntPoint> GoalPositions;       // 缓存：InitFromLevelData 时从 Cells 中扫描 CellType==Goal 得到
// 注意：不再有 GridWidth / GridHeight 成员，边界由 GetGridBounds() 动态计算
```

---

## 二、角色（SokobanCharacter）

### 复用引擎能力

| 引擎能力 | 用途 |
|---------|------|
| ACharacter | 基类，自带胶囊碰撞体、Mesh 挂载 |
| UCharacterMovementComponent | 保留组件但**不用其自由移动**，仅用于地面检测等基础功能 |
| Enhanced Input System | 模板已配置，绑定 WASD 四方向 |
| 动画蓝图 | 如需要走路动画可直接使用模板自带的 |

### 移动逻辑（格子化离散移动）

```
输入 WASD
  → 判断是否正在移动（锁定输入防止连续触发）
  → 计算目标格坐标
  → 调用 GridManager->TryMoveActor()
  → 成功：GridManager 先完成逻辑结算，并广播角色/箱子的目标格
  → 角色收到自己的移动事件后，用 FTimeline 平滑移动到目标世界坐标
  → 移动完成回调：仅解锁输入；踏板、门、通关都已在逻辑结算阶段完成
```

**核心成员：**

```cpp
FIntPoint CurrentGridPos;           // 当前格子坐标缓存；逻辑真相以 GridManager 为准
bool bIsMoving = false;             // 移动锁
float MoveSpeed = 400.0f;          // 视觉移动速度
AGridManager* GridManagerRef;       // 缓存引用

void OnMoveInput(EMoveDirection Dir);   // 输入回调
void SmoothMoveTo(FVector TargetPos);   // 平滑移动
void OnMoveCompleted();                 // 移动完成
```

---

## 三、箱子（PushableBox）

简单 Actor，核心信息由 GridManager 管理，自己不做玩法判定。

```cpp
UCLASS()
class APushableBox : public AActor
{
    UStaticMeshComponent* MeshComp;  // 用引擎 Cube
    FIntPoint CurrentGridPos;       // 当前格子坐标缓存；逻辑真相以 GridManager 为准

    // 和角色一样的平滑移动
    void SmoothMoveTo(FVector TargetPos);
    void OnMoveCompleted();
};
```

箱子不做自主逻辑，所有推动判定在 GridManager 中完成。

---

## 四、机关系统

### 基类 AGridMechanism

```cpp
UCLASS(Abstract)
class AGridMechanism : public AActor
{
    GENERATED_BODY()
public:
    FIntPoint GridPos;
    virtual void OnActivate();      // 机关激活
    virtual void OnDeactivate();    // 机关失活
    virtual bool IsActivated() const;
};
```

### 踏板 APressurePlate

- 继承 AGridMechanism
- **纯表现 Actor**：由 GridManager 在 `InitFromLevelData` 时根据 CellType==PressurePlate 的格子自动生成，不参与逻辑判定
- **`int32 GroupId`** — 分组编号，决定该踏板控制哪些门
- 视觉：扁平 Cylinder，**颜色由 GroupId 查 颜色配置表 决定**（动态材质实例 `SetVectorParameterValue`）
- 激活时颜色变亮/发光，未激活时颜色较暗
- 逻辑：GridManager 每次移动后检查该格是否被箱子**或玩家**占据（`OccupyingActor != nullptr`）
- **布局约束**：同组踏板不应放置在对应门的曼哈顿距离 1 格以内，以避免"门关闭时卡住站在踏板上的实体"等边界情况。编辑器应在保存时校验并警告

### 门 ADoor

- 继承 AGridMechanism
- **纯表现 Actor**：由 GridManager 根据 CellType==Door 的格子自动生成
- **`int32 GroupId`** — 与同组踏板配对
- 一局中可存在**多扇不同分组的门**
- 一扇门可由**多个同组踏板**控制（同组踏板全部激活 → 门开启）
- 监听 GridManager 的 `OnGroupPlatesChanged(int32 GroupId)` 委托
- 视觉：门的 Mesh 颜色与其 GroupId 对应颜色一致，方便玩家识别配对关系
- 开门动画：Timeline 控制 Mesh Z轴位移或缩放到 0
- **CellType 保持为 `Door` 不变**，`GridManager` 通过切换 `FGridCell::bDoorOpen` 控制开关状态，门 Actor 只消费该状态播放表现
- **支持反向关闭**：如果箱子离开踏板导致同组踏板不再全部激活，门重新关闭
- **门上有箱子时的特殊处理**：如果门从开→关时，门格子上正好有箱子（`OccupyingActor != nullptr`），该箱子被视为"卡住"状态——不可被推动，直到门重新打开。这避免了箱子穿越关闭的门的问题

### 目标格 Goal Tile

- 目标格是地块类型，不需要独立机关 Actor
- 视觉上使用高对比度地贴/发光圈，和踏板明显区分
- 逻辑上始终可通行
- **通关条件**：玩家逻辑位置进入 `Goal` 格时，`GridManager` 触发 `OnPlayerEnteredGoal`，`GameMode` 判定当前关卡完成
- 门仍然只负责“解锁路径”，不再承担“所有门打开即通关”的职责

### 分组配对机制

```
每次逻辑移动结算后：
  activeGroups = 收集所有出现过的 GroupId
  for each groupId in activeGroups:
      plates = 所有 GroupId == groupId 的踏板
      allActivated = 所有 plates 的 OccupyingActor != nullptr（箱子或玩家均可激活）
      doors = 所有 GroupId == groupId 的门
      for each door in doors:
          if allActivated: door.Open()
          else: door.Close()
```

### 分组样式定义

颜色配置**不依赖外部 DataTable**，而是作为关卡数据的一部分，**随关卡 JSON 一起保存/加载**，在关卡编辑器中直接管理。

```cpp
// 分组样式（每个关卡自带）
USTRUCT(BlueprintType)
struct FMechanismGroupStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FLinearColor BaseColor;   // 基础颜色
    UPROPERTY(EditAnywhere) FLinearColor ActiveColor;  // 激活颜色（发光）
    UPROPERTY(EditAnywhere) FString DisplayName;       // 编辑器中显示的名称（如 "红门", "出口A"）
};
```

运行时查找逻辑：`GridManager` 从 `FLevelData::GroupStyles` 按 GroupId 查颜色。找不到时 fallback 到哈希生成的随机色，保证不崩溃。

### 冰面 Ice（扩展机关）

- 不需要继承 AGridMechanism（它是地块类型，不是交互机关）
- **玩家和箱子都受冰面影响**
- 逻辑完全在 GridManager 的 `TryMoveActor` 中处理，角色/箱子只接收最终落点并播放滑行动画

**滑行规则：**

```
实体（玩家或箱子）移动到冰面格时：
  currentPos = 当前冰面格
  while true:
      nextPos = currentPos + direction
      if nextPos 的 CellType != Ice:
          break  // 冰面结束，停在 currentPos（无论下一格是 Floor、Wall 还是 Empty）
      if nextPos 有 OccupyingActor:
          break  // 冰面上遇到障碍物，停在 currentPos
      if !IsCellPassable(nextPos):
          break  // 不可通行，停在 currentPos
      currentPos = nextPos
  最终落点 = currentPos
```

**关键规则：**
- 滑行在冰面边界停止——实体停在最后一格冰面上，不会滑到非冰面格子上
- 未被玩家直接推动的箱子视为墙壁/障碍物——滑行中遇到静止的箱子会被阻挡停下，不会连锁推动
- 玩家在冰面上推箱子：箱子先按滑行规则滑到落点，玩家自身也从冰面格开始滑行
- Empty 格（坑洞）不是冰面，滑行不会进入 Empty

---

## 五、地块视觉样式系统

地面和墙壁支持多种视觉样式。样式定义作为**独立的全局配置**（DataAsset），不随关卡保存，而是作为编辑器的可选样式列表供关卡设计者选用。关卡数据只存储每个格子引用的 `StyleId`。

### 设计思路

```
全局样式目录（DataAsset）         关卡数据（JSON）
┌───────────────────────┐        ┌──────────────────────┐
│ "stone_floor"         │        │ cellStyles: [        │
│ "wood_floor"          │   ←引用  │   {pos:[2,3],        │
│ "grass_floor"         │        │    style:"wood_floor"}│
│ "brick_wall"          │        │ ]                    │
│ "stone_wall"          │        └──────────────────────┘
│ ...                   │
└───────────────────────┘
```

- **全局配置** 定义"有哪些样式可选"，包含 Mesh、材质等资源引用
- **关卡数据** 只记录"某个格子用了哪个样式"，用 `FName StyleId` 引用
- 未指定样式的格子使用该类型的默认样式

### UTileStyleCatalog — 全局样式目录（DataAsset）

```cpp
USTRUCT(BlueprintType)
struct FTileVisualStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName StyleId;               // 唯一标识，如 "stone_floor", "brick_wall"
    UPROPERTY(EditAnywhere) FString DisplayName;          // 编辑器中显示的名称，如 "石砖地面"
    UPROPERTY(EditAnywhere) EGridCellType ApplicableType; // 适用的格子类型（Floor / Wall）
    UPROPERTY(EditAnywhere) UStaticMesh* Mesh;            // 可选，覆盖默认 Mesh
    UPROPERTY(EditAnywhere) UMaterialInterface* Material; // 可选，覆盖默认材质
    UPROPERTY(EditAnywhere) UTexture2D* Thumbnail;        // 编辑器侧边栏缩略图
};

UCLASS()
class UTileStyleCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    TArray<FTileVisualStyle> Styles;

    // 按 StyleId 查找，找不到返回 nullptr
    const FTileVisualStyle* FindStyle(FName StyleId) const;

    // 返回所有适用于指定类型的样式列表（供编辑器 UI 展示）
    TArray<const FTileVisualStyle*> GetStylesForType(EGridCellType Type) const;
};
```

**配置方式**：在 UE 编辑器中创建 `UTileStyleCatalog` DataAsset，添加各种地面/墙壁样式条目。项目中可以有多个 Catalog（如"古堡主题"、"冰雪主题"），通过 GameInstance 或项目设置指定当前使用哪一个。

### 关卡数据中的样式引用

关卡 JSON 只存储有自定义样式的格子，未指定的格子使用默认样式：

```cpp
USTRUCT()
struct FCellStyleData
{
    GENERATED_BODY()

    FIntPoint GridPos;
    FName VisualStyleId;    // 引用 UTileStyleCatalog 中的 StyleId
};
```

### GridManager 生成 Actor 时的样式查找流程

```
生成地面/墙壁 Actor 时：
  → 读取 FGridCell::VisualStyleId
  → 如果 != NAME_None → 从 UTileStyleCatalog 查找对应样式
  → 找到 → 使用样式中的 Mesh / Material
  → 找不到或 NAME_None → 使用该类型的默认 Mesh / Material（Plane / Cube）
```

### 编辑器集成

编辑器侧边栏在选中 Floor 或 Wall 笔刷时，显示样式选择面板：

```
┌─ 地面样式 ─────────────────────┐
│                                 │
│  [默认]  [石砖]  [木地板]  [草地] │
│   ☑        ☐       ☐       ☐   │
│                                 │
│  （缩略图 + 名称，单选）          │
└─────────────────────────────────┘
```

- 选中样式后，绘制对应类型格子时自动附带该 `VisualStyleId`
- 选"默认"即不写入 StyleId，使用引擎基础几何体

---

## 六、关卡数据系统

### FLevelData 结构

关卡数据采用**稀疏格子列表**，无固定宽高，地图形状完全由格子集合决定：

```cpp
USTRUCT()
struct FMechanismGroupStyleData
{
    GENERATED_BODY()

    int32 GroupId = -1;
    FLinearColor BaseColor;
    FLinearColor ActiveColor;
    FString DisplayName;
};

USTRUCT()
struct FCellData
{
    GENERATED_BODY()

    FIntPoint GridPos;
    FString CellType;                    // 稳定字符编码："Floor", "Wall", "Ice", "Goal", "Door", "PressurePlate" 等
    FName VisualStyleId = NAME_None;     // 可选，视觉样式引用
    int32 GroupId = -1;                  // 可选，仅踏板/门有效
    int32 ExtraParam = 0;               // 可选，扩展参数
};

USTRUCT()
struct FLevelData
{
    GENERATED_BODY()

    TArray<FCellData> Cells;             // 稀疏格子列表，仅包含有效格子（无固定宽高）
    FIntPoint PlayerStart;               // 玩家起点
    // GoalPos 不再单独存储：由 InitFromLevelData 时从 Cells 中扫描 CellType==Goal 的格子得到，缓存在 GridManager 中
    TArray<FIntPoint> BoxPositions;      // 箱子位置列表
    TArray<FMechanismGroupStyleData> GroupStyles; // 分组样式列表
};
```

### JSON 格式示例

```json
{
  "cells": [
    { "gridPos": [0, 0], "cellType": "Wall" },
    { "gridPos": [1, 0], "cellType": "Wall" },
    { "gridPos": [2, 0], "cellType": "Wall" },
    { "gridPos": [1, 1], "cellType": "Floor", "visualStyleId": "wood_floor" },
    { "gridPos": [2, 1], "cellType": "Floor", "visualStyleId": "wood_floor" },
    { "gridPos": [3, 1], "cellType": "Floor" },
    { "gridPos": [2, 2], "cellType": "PressurePlate", "groupId": 0 },
    { "gridPos": [7, 2], "cellType": "PressurePlate", "groupId": 0 },
    { "gridPos": [2, 5], "cellType": "PressurePlate", "groupId": 1 },
    { "gridPos": [6, 5], "cellType": "PressurePlate", "groupId": 1 },
    { "gridPos": [3, 7], "cellType": "Door", "groupId": 0 },
    { "gridPos": [6, 7], "cellType": "Door", "groupId": 1 },
    { "gridPos": [8, 7], "cellType": "Goal" }
  ],
  "playerStart": [1, 1],
  "boxes": [[3, 3], [4, 2], [5, 5], [3, 6]],
  "groupStyles": [
    { "groupId": 0, "displayName": "红门", "baseColor": [1, 0.1, 0.1], "activeColor": [1, 0.4, 0.4] },
    { "groupId": 1, "displayName": "蓝门", "baseColor": [0.1, 0.3, 1], "activeColor": [0.4, 0.6, 1] }
  ]
}
```

`cells` 使用稳定字符串编码 `cellType`，不依赖 `EGridCellType` 的枚举序号：

- `"Wall"` / `"Floor"` / `"PressurePlate"` / `"Ice"` / `"Door"` / `"Goal"`
- 只存储有效格子，不存在于列表中的坐标视为 Empty（虚空/不可通行）
- 地图形状完全由 `cells` 列表决定，无固定矩形约束，支持 L 形、环形等任意形状

**示例说明**：上例为简化版（省略了大量 Floor/Wall 格子），完整关卡会包含所有非 Empty 格子。Group 0 的 2 个踏板 `[2,2]` `[7,2]` 控制 Group 0 的门 `[3,7]`，Group 1 的 2 个踏板 `[2,5]` `[6,5]` 控制 Group 1 的门 `[6,7]`。目标格 `[8,7]` 直接作为 CellType=="Goal" 存在于 cells 列表中，不再需要单独的 `goalPos` 字段。
分组颜色随关卡一起保存在 `groupStyles` 中，在关卡编辑器里直接配置。

### LevelSerializer

```cpp
class ULevelSerializer
{
public:
    static bool SaveToJson(const FLevelData& Data, const FString& FilePath);
    static bool LoadFromJson(const FString& FilePath, FLevelData& OutData);
};
```

优先使用 `FJsonObjectConverter` 处理这些显式结构，避免手写复杂的 Map-Key 转换逻辑。存储路径 `FPaths::ProjectSavedDir() / TEXT("Levels/")`。

---

## 七、Runtime 关卡编辑器

### ALevelEditorGameMode

独立的 GameMode，与游戏主 GameMode 分离。

**模式切换机制**：通过 `UGameplayStatics::OpenLevel` 加载不同的 Map 来切换 GameMode。编辑器和游戏使用**不同的关卡 Map**，各自在 WorldSettings 中指定对应的 GameMode：
- 主菜单 → 选择"编辑关卡" → OpenLevel 加载编辑器 Map（使用 ALevelEditorGameMode）
- 主菜单 → 选择"开始游戏" → OpenLevel 加载游戏 Map（使用 ASokobanGameMode）
- 编辑器中"测试关卡"按钮 → 先将当前编辑状态序列化为临时 JSON → OpenLevel 切换到游戏 Map → GameMode 读取临时 JSON 加载关卡
- 游戏中"返回编辑"（如果从编辑器进入）→ OpenLevel 切回编辑器 Map → 重新加载临时 JSON 恢复编辑状态

### ALevelEditorPawn

- 替代角色的编辑器控制器
- 俯视正交相机，支持滚轮缩放、中键拖拽平移

### 编辑器工作流程

```
新建关卡：
  → 弹出对话框，输入初始宽度和高度（如 8×6）
  → 调用 GridManager->InitEmptyGrid(Width, Height)
  → 生成对应尺寸的纯 Floor 矩形区域作为起始画布
  → 进入编辑模式，可自由绘制

加载关卡：
  → 读取 JSON，调用 GridManager->InitFromLevelData()
  → 从稀疏格子数据重建场景
```

### 编辑器功能清单

| 功能 | 实现方式 |
|------|---------|
| 笔刷选择 | UMG 侧边栏，按钮切换当前笔刷类型（地板/墙/箱子/起点/目标格/冰面/门/橡皮擦）。注意：踏板没有独立笔刷，而是通过门的放置流程自动关联（见下方"门与踏板的放置流程"）|
| 样式选择 | 选中地板或墙壁笔刷时，显示样式面板，从 `UTileStyleCatalog` 中列出可选样式（缩略图+名称） |
| 分组管理面板 | 侧边栏下方的分组管理区域（见下方详细说明） |
| 左键绘制 | 点击或拖拽放置当前笔刷对应元素；**地板笔刷可以在空白区域（Empty）绘制，从而向外扩展地图面积** |
| 右键擦除/橡皮擦 | 点击或拖拽将格子从网格中移除（变为 Empty/虚空），从而**缩减地图面积**；如果该格子上有箱子/机关等，一并清除 |
| 保存 | 序列化当前编辑状态为 JSON（稀疏格子列表 + 分组样式） |
| 加载 | 读取 JSON 重建编辑场景（恢复分组样式） |
| 测试 | 一键从编辑器切换到游戏模式测试当前关卡 |

### 地图面积动态编辑

编辑器的核心交互：**地图没有固定边界，笔刷即是地图的塑形工具**。

```
扩展地图：
  → 选择「地板」笔刷
  → 在现有地图边缘外的空白区域（Empty）点击/拖拽
  → GridManager.SetCell() 在该坐标创建新的 Floor 格子
  → 生成对应的地面可视化 Actor
  → 地图面积自然增长，支持任意不规则形状

缩减地图：
  → 选择「橡皮擦」笔刷 或 使用右键
  → 在现有格子上点击/拖拽
  → GridManager.RemoveCell() 删除该坐标的格子
  → 销毁对应的可视化 Actor
  → 该位置变为虚空（Empty），地图面积自然缩小

安全检查：
  → 擦除时如果该格子上有玩家起点/箱子/机关，弹出确认提示
  → 擦除后自动检查地图连通性（可选警告，不强制阻止）
```

**编辑器辅助网格**：在编辑模式下，当前地图包围盒周围额外显示一圈半透明的辅助网格线，提示用户可以在这些位置扩展地图。辅助网格随地图边界动态更新。

### 分组管理面板

位于编辑器侧边栏，笔刷选择区域下方。所有分组配置**随关卡保存**，不同关卡可以有完全不同的分组方案。

```
┌─ 分组管理 ──────────────────┐
│                              │
│  [+ 新建分组]                │
│                              │
│  ■ Group 0: "红门"           │
│    颜色: [■ 拾色器]  [删除]  │
│                              │
│  ■ Group 1: "蓝门"           │
│    颜色: [■ 拾色器]  [删除]  │
│                              │
│  ■ Group 2: "出口"           │
│    颜色: [■ 拾色器]  [删除]  │
│                              │
└──────────────────────────────┘
```

| 操作 | 行为 |
|------|------|
| 新建分组 | 自动分配下一个可用 GroupId，给予默认颜色（从预设色板循环取色）和默认名称 |
| 编辑名称 | 点击名称文字直接内联编辑 |
| 拾色器 | 点击色块弹出 UMG 颜色选择器，修改后实时更新场景中所有同组踏板/门的颜色 |
| 删除分组 | 删除分组时，如果场景中仍有该组的踏板/门，弹出确认提示并一并清除 |
| 选中分组 | 点击分组行 → 当前笔刷绑定该 GroupId，后续放置踏板/门自动使用此分组 |

**预设色板**（新建分组时自动循环分配）：红、蓝、绿、黄、紫、橙、青、粉。策划可通过拾色器覆盖为任意颜色。

### 门与踏板的放置流程

门和踏板通过**关联放置**的方式操作，不需要手动管理分组编号：

```
放置新门：
  → 选择「门」笔刷，点击目标格子放置门
  → 自动创建新分组（分配 GroupId，默认颜色/名称）
  → 自动切换为该门的「踏板放置」模式（侧边栏提示"正在为 [红门] 放置踏板"）
  → 点击其他格子放置该分组的踏板（可放置多个）
  → 按 Esc 或点击其他笔刷退出踏板放置模式

编辑已有门：
  → 左键点击已放置的门 Actor
  → 进入该门的「踏板放置/移除」模式
  → 点击空格子：为该分组新增踏板
  → 点击已有的同组踏板：移除该踏板
  → 按 Esc 退出

删除门：
  → 用橡皮擦/右键擦除门格子
  → 弹出确认提示：是否同时删除该分组所有踏板？
  → 确认后删除门 + 同组所有踏板，移除分组
```

### 起点与目标格的放置

- **起点笔刷**：场景中只允许一个起点，放置新起点时自动将旧起点格改为 Floor
- **目标格笔刷**：场景中只允许一个目标格，放置新目标格时自动将旧目标格改为 Floor

### 核心交互流程

```
鼠标点击/拖拽
  → 射线检测获取命中世界坐标（对于空白区域，射线打到地面参考平面上）
  → WorldToGrid 转换为格子坐标
  → 左键 + 笔刷绘制：
      → 如果是地板/墙/冰面笔刷：调用 SetCell() 创建或覆盖格子（支持在 Empty 区域扩展）
      → 如果是门笔刷：放置门并自动进入踏板放置模式（见上方流程）
      → 如果处于踏板放置模式：在目标格放置同组踏板
      → 如果是目标格笔刷：放置 CellType==Goal（场景唯一，自动替换旧的）
      → 如果是起点笔刷：更新 PlayerStart（场景唯一，自动替换旧的）
      → 如果选中了非默认样式，写入 VisualStyleId
  → 右键 / 橡皮擦绘制：
      → 调用 RemoveCell() 删除该坐标格子，销毁对应 Actor
  → 同步更新场景可视化 Actor + 辅助网格范围
```

---

## 八、游戏框架

### ASokobanGameMode

| 职责 | 说明 |
|------|------|
| 关卡加载 | 调用 LevelSerializer 读取 JSON，传给 GridManager 初始化 |
| 流程控制 | 监听 `OnPlayerEnteredGoal`，玩家到达目标格后显示过关 UI、加载下一关 |
| 重置 | 重新加载当前关卡 JSON，恢复初始状态 |

### ASokobanGameState

| 数据 | 说明 |
|------|------|
| 当前步数 | `int32 StepCount` |
| 撤销栈 | `TArray<FLevelSnapshot> UndoStack`，每步记录玩家位置、箱子位置和步数快照 |
| 通关状态 | `bool bLevelCompleted` |

### 撤销系统

```cpp
USTRUCT()
struct FDoorSnapshot
{
    FIntPoint DoorPos;
    bool bDoorOpen = false;
};

USTRUCT()
struct FPitSnapshot
{
    FIntPoint PitPos;
    bool bFilled = false;   // true = 已被填平（当前为 Floor），false = 仍为坑洞（Empty）
};

USTRUCT()
struct FLevelSnapshot
{
    FIntPoint PlayerPos;
    TArray<FIntPoint> BoxPositions;      // 包括尚存的所有箱子（被坑洞吞掉的不在列表中）
    TArray<FDoorSnapshot> DoorStates;    // 所有门的开关状态
    TArray<FPitSnapshot> PitStates;      // 所有坑洞格的填平状态（用于撤销填坑操作）
    int32 StepCount = 0;
};

// 每次移动前压入
UndoStack.Push(CurrentSnapshot());

// 撤销时弹出恢复
FLevelSnapshot Last = UndoStack.Pop();
RestoreSnapshot(Last);
```

`RestoreSnapshot()` 是一次完整的状态重建流程：

1. 清空当前占用表
2. 恢复坑洞状态：根据 `PitStates` 将已填平的格子恢复为 Empty 或保持为 Floor（可能需要重新生成/销毁被填坑的箱子 Actor）
3. 恢复玩家/箱子逻辑坐标（按快照中的 BoxPositions 重建，被坑洞吞掉的箱子会被重新生成）
4. 重建所有 `OccupyingActor`
5. 恢复所有门的 `bDoorOpen` 状态（从快照中直接读取，确保与快照时刻完全一致）
6. 根据恢复后的箱子/玩家位置重新计算踏板激活状态
7. 通知所有 Actor 直接 Snap 到恢复后的位置/状态

---

## 九、UE 引擎复用清单

| 引擎能力 | 用于 |
|---------|------|
| ACharacter + CapsuleComponent | 角色基础 |
| Enhanced Input System | WASD 输入绑定 |
| StaticMesh 基本几何体 | Cube→墙/箱子, Plane→地板, Cylinder→踏板 |
| 动态材质实例 | 踏板激活变色、箱子到位反馈 |
| UMG (Widget Blueprint) | 编辑器 UI、游戏 HUD、过关面板 |
| FTimeline | 平滑移动插值、门开关动画 |
| JsonUtilities 模块 | JSON 序列化/反序列化 |
| GameMode / GameState | 游戏流程和状态管理 |
| PlayerController | 输入路由，编辑器/游戏双模式切换 |
| UDataAsset | 地块视觉样式目录（TileStyleCatalog）|

---

## 十、关卡设计规划

| 关卡 | 目标 | 要素 |
|------|------|------|
| Level 1 — 教学 | 教会基础操作与目标格通关 | 1个箱子 + 1个踏板 + 1扇门（同组）+ 1个目标格，开门后走到目标格过关 |
| Level 2 — 双组门 | 引入多门多组机制 | 2个箱子 + 2组各1踏板1门 + 1个目标格，需分别解锁路径 |
| Level 3 — 多开关联动 | 一门多踏板 | 3个箱子 + 1组2踏板控制1扇门 + 冰面区域 + 1个目标格 |
| Level 4 — 综合挑战 | 综合考验 | 4+箱子 + 3组门 + 混合地形 + 目标格位于最终解锁区域 |

---

## 十一、开发优先级与排期

| 天数 | 任务 | 产出 |
|------|------|------|
| Day 1 | Grid 系统 + 坐标转换 + 基础可视化 | 能看到网格地图 |
| Day 2 | 角色格子移动 + 推箱子逻辑 | 能推箱子 |
| Day 3 | 踏板 + 门 + 目标格 + 通关检测 | 基础玩法闭环 |
| Day 4 | JSON 存取（稳定结构） + 关卡编辑器核心 | 能刷地图并保存 |
| Day 5 | 编辑器 UI 完善 + 测试功能 | 编辑器可用 |
| Day 6 | 冰面机关 + 3 个关卡设计 | 扩展玩法 + 内容 |
| Day 7 | 视觉反馈 + 撤销 + 整体测试打磨 | 完整可交付 Demo |
