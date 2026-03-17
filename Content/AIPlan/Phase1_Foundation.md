# Phase 1: Foundation — 详细实施计划

## 概述

Phase 1 建立项目的基础设施层，包含三个子任务。1A 可独立开发；1B 的头文件可通过前向声明独立编译，但 1B.cpp 中 `InitFromLevelData` 的实现依赖 1C 的 `LevelDataTypes.h`，因此建议 1A 和 1C 先行，1B 的 `InitFromLevelData` 实现在 1C 完成后补充。项目模块名为 `TuiXiangZi`，基于 UE5.5.4 Top Down 模板。所有新代码放在 `Source/TuiXiangZi/` 下按功能分子目录组织。

## 目录结构约定

```
Source/TuiXiangZi/
├── TuiXiangZi.h                    # 已有，模块头文件
├── TuiXiangZi.cpp                  # 已有，模块实现
├── TuiXiangZi.Build.cs             # 已有，需修改添加依赖
├── Grid/
│   ├── GridTypes.h                  # 1A: 基础数据定义
│   ├── GridTypes.cpp                # 1A: 字符串编码实现
│   ├── GridManager.h                # 1B: 网格管理器声明
│   └── GridManager.cpp              # 1B: 网格管理器实现
└── LevelData/
    ├── LevelDataTypes.h             # 1C: 关卡数据结构定义
    ├── LevelSerializer.h            # 1C: 序列化器声明
    └── LevelSerializer.cpp          # 1C: 序列化器实现
```

---

## 子任务 1A: 项目脚手架 + GridTypes 基础数据定义

### 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `Source/TuiXiangZi/TuiXiangZi.Build.cs` | **修改** | 添加 `Json`, `JsonUtilities` 模块依赖 |
| `Source/TuiXiangZi/Grid/GridTypes.h` | **新建** | 基础类型定义 |
| `Source/TuiXiangZi/Grid/GridTypes.cpp` | **新建** | 字符串编码实现 |

### 1A-1: TuiXiangZi.Build.cs 修改

在 `PublicDependencyModuleNames` 中追加 `"Json"` 和 `"JsonUtilities"`：

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore",
    "NavigationSystem", "AIModule", "Niagara", "EnhancedInput",
    "Json", "JsonUtilities"  // 新增：JSON 序列化支持
});
```

### 1A-2: Grid/GridTypes.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GridTypes.generated.h"

UENUM(BlueprintType)
enum class EGridCellType : uint8
{
    Empty           UMETA(DisplayName = "Empty"),
    Floor           UMETA(DisplayName = "Floor"),
    Wall            UMETA(DisplayName = "Wall"),
    PressurePlate   UMETA(DisplayName = "PressurePlate"),
    Ice             UMETA(DisplayName = "Ice"),
    Goal            UMETA(DisplayName = "Goal"),
    Door            UMETA(DisplayName = "Door"),
};

UENUM(BlueprintType)
enum class EMoveDirection : uint8
{
    Up      UMETA(DisplayName = "Up"),
    Down    UMETA(DisplayName = "Down"),
    Left    UMETA(DisplayName = "Left"),
    Right   UMETA(DisplayName = "Right"),
};

USTRUCT(BlueprintType)
struct FGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    EGridCellType CellType = EGridCellType::Empty;

    // 使用 TWeakObjectPtr 避免阻止 GC，不加 UPROPERTY（运行时动态引用）
    TWeakObjectPtr<AActor> OccupyingActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    bool bDoorOpen = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GroupId = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 ExtraParam = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    FName VisualStyleId = NAME_None;
};

// 坐标系约定：X 向右 (Right)，Y 向上 (Up)
FORCEINLINE FIntPoint DirectionToOffset(EMoveDirection Dir)
{
    switch (Dir)
    {
    case EMoveDirection::Up:    return FIntPoint(0, 1);
    case EMoveDirection::Down:  return FIntPoint(0, -1);
    case EMoveDirection::Left:  return FIntPoint(-1, 0);
    case EMoveDirection::Right: return FIntPoint(1, 0);
    default:                    return FIntPoint(0, 0);
    }
}

namespace GridTypeUtils
{
    FString CellTypeToString(EGridCellType Type);
    EGridCellType StringToCellType(const FString& Str);
}
```

### 1A-3: Grid/GridTypes.cpp

```cpp
#include "Grid/GridTypes.h"

namespace GridTypeUtils
{
    static const TMap<EGridCellType, FString> TypeToStringMap = {
        { EGridCellType::Empty,         TEXT("Empty") },
        { EGridCellType::Floor,         TEXT("Floor") },
        { EGridCellType::Wall,          TEXT("Wall") },
        { EGridCellType::PressurePlate, TEXT("PressurePlate") },
        { EGridCellType::Ice,           TEXT("Ice") },
        { EGridCellType::Goal,          TEXT("Goal") },
        { EGridCellType::Door,          TEXT("Door") },
    };

    FString CellTypeToString(EGridCellType Type)
    {
        if (const FString* Found = TypeToStringMap.Find(Type))
        {
            return *Found;
        }
        return TEXT("Empty");
    }

    EGridCellType StringToCellType(const FString& Str)
    {
        for (const auto& Pair : TypeToStringMap)
        {
            if (Pair.Value == Str) return Pair.Key;
        }
        UE_LOG(LogTemp, Warning, TEXT("GridTypeUtils::StringToCellType: Unknown type '%s', defaulting to Empty"), *Str);
        return EGridCellType::Empty;
    }
}
```

### 1A 关键注意事项

1. **UMETA(DisplayName)**: 每个枚举值加 DisplayName 以便蓝图编辑器显示友好名称
2. **TWeakObjectPtr 不加 UPROPERTY**: `OccupyingActor` 不需要序列化，也不应阻止 GC
3. **GENERATED_BODY()**: 所有 USTRUCT 和 UENUM 必须有此宏
4. **include 路径**: 子目录文件使用 `"Grid/GridTypes.h"` 格式
5. **坐标系约定**: `FIntPoint(X, Y)` 中 X 对应世界 X 轴（右），Y 对应世界 Y 轴（前/上）

### 1A 验收标准

1. 项目编译通过，无错误无警告
2. 可以声明 `FGridCell` 变量并赋值各字段
3. `DirectionToOffset(EMoveDirection::Up)` 返回 `FIntPoint(0, 1)`
4. `GridTypeUtils::CellTypeToString(EGridCellType::Wall)` 返回 `"Wall"`
5. `GridTypeUtils::StringToCellType("PressurePlate")` 返回 `EGridCellType::PressurePlate`
6. `GridTypeUtils::StringToCellType("InvalidType")` 返回 `EGridCellType::Empty` 并输出警告

---

## 子任务 1B: GridManager 核心

### 依赖 1A 的类型

| 来自 1A | 用途 |
|---------|------|
| `EGridCellType` | 判断格子可通行性 |
| `FGridCell` | TMap 的 Value 类型 |
| `EMoveDirection` | TryMoveActor 参数（Phase 1 仅声明） |
| `DirectionToOffset()` | 方向转偏移量 |
| `GridTypeUtils::StringToCellType()` | InitFromLevelData 中转换字符串 |

### 文件清单

| 文件路径 | 操作 | 说明 |
|----------|------|------|
| `Source/TuiXiangZi/Grid/GridManager.h` | 新建 | AGridManager 声明 |
| `Source/TuiXiangZi/Grid/GridManager.cpp` | 新建 | AGridManager 实现 |

### Grid/GridManager.h 完整签名

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridTypes.h"
#include "GridManager.generated.h"

struct FLevelData;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActorLogicalMoved, AActor*, FIntPoint, FIntPoint);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerEnteredGoal, FIntPoint);

UCLASS(BlueprintType)
class TUIXIANGZI_API AGridManager : public AActor
{
    GENERATED_BODY()

public:
    AGridManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ===== 网格配置 =====
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
    float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid|Config")
    FVector GridOrigin = FVector::ZeroVector;

    // ===== 坐标转换 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
    FVector GridToWorld(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Coordinate")
    FIntPoint WorldToGrid(FVector WorldPos) const;

    // ===== 格子查询 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    FGridCell GetCell(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool HasCell(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool IsCellPassable(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool CanPushBoxTo(FIntPoint GridPos) const;

    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    bool IsCellOccupied(FIntPoint GridPos) const;

    // ===== 格子增删 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Edit")
    void SetCell(FIntPoint GridPos, const FGridCell& Cell);

    UFUNCTION(BlueprintCallable, Category = "Grid|Edit")
    void RemoveCell(FIntPoint GridPos);

    // ===== 动态边界 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Query")
    FIntRect GetGridBounds() const;

    // ===== 关卡管理 =====
    void InitFromLevelData(const FLevelData& Data);

    UFUNCTION(BlueprintCallable, Category = "Grid|Level")
    void InitEmptyGrid(int32 Width, int32 Height);

    UFUNCTION(BlueprintCallable, Category = "Grid|Level")
    void ClearGrid();

    // ===== 事件委托 =====
    FOnActorLogicalMoved OnActorLogicalMoved;
    FOnPlayerEnteredGoal OnPlayerEnteredGoal;

    // ===== Phase 3 预留 =====
    UFUNCTION(BlueprintCallable, Category = "Grid|Gameplay")
    bool TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction);

protected:
    UPROPERTY(VisibleAnywhere, Category = "Grid|Debug")
    TMap<FIntPoint, FGridCell> GridCells;

    UPROPERTY(VisibleAnywhere, Category = "Grid|Debug")
    TArray<FIntPoint> GoalPositions;

    UPROPERTY()
    TMap<FIntPoint, AActor*> VisualActors;

    void SpawnOrUpdateVisualActor(FIntPoint GridPos, const FGridCell& Cell);
    void DestroyVisualActor(FIntPoint GridPos);
    void DestroyAllVisualActors();
    UStaticMesh* GetDefaultMeshForCellType(EGridCellType CellType) const;
    UMaterialInterface* GetDefaultMaterialForCellType(EGridCellType CellType) const;

    UPROPERTY(EditAnywhere, Category = "Grid|Debug")
    bool bDrawDebugGrid = true;

    UPROPERTY(EditAnywhere, Category = "Grid|Debug")
    FColor DebugGridColor = FColor::White;

    void DrawDebugGridLines() const;
};
```

### Grid/GridManager.cpp 关键实现

```cpp
#include "Grid/GridManager.h"
#include "LevelData/LevelDataTypes.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"

AGridManager::AGridManager()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AGridManager::BeginPlay() { Super::BeginPlay(); }
void AGridManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearGrid();
    Super::EndPlay(EndPlayReason);
}

// ---- 坐标转换 ----
FVector AGridManager::GridToWorld(FIntPoint GridPos) const
{
    return GridOrigin + FVector(
        GridPos.X * CellSize + CellSize * 0.5f,
        GridPos.Y * CellSize + CellSize * 0.5f,
        0.0f);
}

FIntPoint AGridManager::WorldToGrid(FVector WorldPos) const
{
    const FVector Relative = WorldPos - GridOrigin;
    return FIntPoint(
        FMath::FloorToInt(Relative.X / CellSize),
        FMath::FloorToInt(Relative.Y / CellSize));
}

// ---- 格子查询 ----
FGridCell AGridManager::GetCell(FIntPoint GridPos) const
{
    if (const FGridCell* Found = GridCells.Find(GridPos)) return *Found;
    return FGridCell(); // 默认 Empty
}

bool AGridManager::HasCell(FIntPoint GridPos) const { return GridCells.Contains(GridPos); }

bool AGridManager::IsCellPassable(FIntPoint GridPos) const
{
    if (!HasCell(GridPos)) return false;
    const FGridCell& Cell = GridCells[GridPos];
    switch (Cell.CellType)
    {
    case EGridCellType::Wall: return false;
    case EGridCellType::Door: return Cell.bDoorOpen;
    case EGridCellType::Floor:
    case EGridCellType::PressurePlate:
    case EGridCellType::Ice:
    case EGridCellType::Goal: return true;
    default: return false;
    }
}

bool AGridManager::CanPushBoxTo(FIntPoint GridPos) const
{
    if (IsCellPassable(GridPos)) return true;
    if (!HasCell(GridPos)) return true; // Empty = 坑洞可填平
    return false;
}

bool AGridManager::IsCellOccupied(FIntPoint GridPos) const
{
    if (const FGridCell* Found = GridCells.Find(GridPos))
        return Found->OccupyingActor.IsValid();
    return false;
}

// ---- 格子增删 ----
void AGridManager::SetCell(FIntPoint GridPos, const FGridCell& Cell)
{
    GridCells.Add(GridPos, Cell);
    SpawnOrUpdateVisualActor(GridPos, Cell);
}

void AGridManager::RemoveCell(FIntPoint GridPos)
{
    GridCells.Remove(GridPos);
    DestroyVisualActor(GridPos);
}

// ---- 动态边界 ----
FIntRect AGridManager::GetGridBounds() const
{
    if (GridCells.Num() == 0) return FIntRect(FIntPoint(0,0), FIntPoint(0,0));
    FIntPoint Min(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
    FIntPoint Max(TNumericLimits<int32>::Min(), TNumericLimits<int32>::Min());
    for (const auto& Pair : GridCells)
    {
        Min.X = FMath::Min(Min.X, Pair.Key.X);
        Min.Y = FMath::Min(Min.Y, Pair.Key.Y);
        Max.X = FMath::Max(Max.X, Pair.Key.X);
        Max.Y = FMath::Max(Max.Y, Pair.Key.Y);
    }
    return FIntRect(Min, Max + FIntPoint(1, 1)); // Max exclusive
}

// ---- 关卡管理 ----
void AGridManager::InitFromLevelData(const FLevelData& Data)
{
    ClearGrid();
    GridOrigin = FVector::ZeroVector;
    for (const FCellData& CellData : Data.Cells)
    {
        FGridCell Cell;
        Cell.CellType = GridTypeUtils::StringToCellType(CellData.CellType);
        Cell.VisualStyleId = CellData.VisualStyleId;
        Cell.GroupId = CellData.GroupId;
        Cell.ExtraParam = CellData.ExtraParam;
        SetCell(CellData.GridPos, Cell);
        if (Cell.CellType == EGridCellType::Goal)
            GoalPositions.Add(CellData.GridPos);
    }
}

void AGridManager::InitEmptyGrid(int32 Width, int32 Height)
{
    ClearGrid();
    for (int32 Y = 0; Y < Height; ++Y)
        for (int32 X = 0; X < Width; ++X)
        {
            FGridCell Cell;
            Cell.CellType = EGridCellType::Floor;
            SetCell(FIntPoint(X, Y), Cell);
        }
}

void AGridManager::ClearGrid()
{
    DestroyAllVisualActors();
    GridCells.Empty();
    GoalPositions.Empty();
}

bool AGridManager::TryMoveActor(FIntPoint FromGrid, EMoveDirection Direction)
{
    // Phase 1: 占位，Phase 3 实现完整逻辑
    return false;
}

// ---- 可视化 Actor 管理 ----
void AGridManager::SpawnOrUpdateVisualActor(FIntPoint GridPos, const FGridCell& Cell)
{
    DestroyVisualActor(GridPos);
    if (Cell.CellType == EGridCellType::Empty) return;
    // 机关类型由 Phase 3B 的专用 Actor (APressurePlate/ADoor) 负责可视化，此处跳过
    if (Cell.CellType == EGridCellType::PressurePlate || Cell.CellType == EGridCellType::Door) return;
    UWorld* World = GetWorld();
    if (!World) return;
    FVector WorldPos = GridToWorld(GridPos);
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* VisualActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(WorldPos), SpawnParams);
    if (!VisualActor) return;
    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(VisualActor);
    MeshComp->RegisterComponent();
    VisualActor->SetRootComponent(MeshComp);
    UStaticMesh* Mesh = GetDefaultMeshForCellType(Cell.CellType);
    if (Mesh) MeshComp->SetStaticMesh(Mesh);
    UMaterialInterface* Material = GetDefaultMaterialForCellType(Cell.CellType);
    if (Material) MeshComp->SetMaterial(0, Material);
    switch (Cell.CellType)
    {
    case EGridCellType::Wall:
        MeshComp->SetWorldScale3D(FVector(CellSize / 100.0f));
        VisualActor->SetActorLocation(WorldPos + FVector(0, 0, CellSize * 0.5f));
        break;
    default:
        MeshComp->SetWorldScale3D(FVector(CellSize / 100.0f, CellSize / 100.0f, 1.0f));
        VisualActor->SetActorLocation(WorldPos);
        break;
    }
    VisualActors.Add(GridPos, VisualActor);
}

void AGridManager::DestroyVisualActor(FIntPoint GridPos)
{
    if (AActor** Found = VisualActors.Find(GridPos))
    {
        if (*Found) (*Found)->Destroy();
        VisualActors.Remove(GridPos);
    }
}

void AGridManager::DestroyAllVisualActors()
{
    for (auto& Pair : VisualActors)
        if (Pair.Value) Pair.Value->Destroy();
    VisualActors.Empty();
}

UStaticMesh* AGridManager::GetDefaultMeshForCellType(EGridCellType CellType) const
{
    switch (CellType)
    {
    case EGridCellType::Wall:
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    default:
        return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    }
}

UMaterialInterface* AGridManager::GetDefaultMaterialForCellType(EGridCellType CellType) const
{
    return nullptr; // Phase 1 使用默认材质
}
```

### 1B 关键注意事项

1. **TUIXIANGZI_API 宏**: 导出宏必须加，否则其他模块无法链接
2. **PrimaryActorTick**: 设 `bCanEverTick = false` 避免不必要开销
3. **LoadObject 路径**: 引擎内置 Mesh 路径 `/Engine/BasicShapes/Cube.Cube`
4. **FIntRect 语义**: `Max` 是 exclusive（不包含）
5. **TMap<FIntPoint, FGridCell>**: `FIntPoint` 已有内置 `GetTypeHash()` 支持

### 1B 验收标准

1. 项目编译通过
2. 在关卡中放置 `AGridManager` Actor 运行不崩溃
3. `InitEmptyGrid(5, 5)` 后场景中出现 5x5 地板
4. `GridToWorld(FIntPoint(0, 0))` 返回 `(50, 50, 0)`（默认 CellSize=100）
5. `WorldToGrid(FVector(150, 250, 0))` 返回 `FIntPoint(1, 2)`
6. `SetCell` / `RemoveCell` 正确增删格子和可视化 Actor
7. `IsCellPassable` 对 Wall 返回 false，Floor 返回 true
8. `CanPushBoxTo` 对不存在坐标返回 true（坑洞可填平）
9. `GetGridBounds()` 正确返回 AABB
10. `ClearGrid()` 后无残留 Actor

---

## 子任务 1C: LevelData 数据类型 + LevelSerializer

### 文件清单

| 文件路径 | 操作 | 说明 |
|----------|------|------|
| `Source/TuiXiangZi/LevelData/LevelDataTypes.h` | 新建 | 关卡数据结构 |
| `Source/TuiXiangZi/LevelData/LevelSerializer.h` | 新建 | 序列化器声明 |
| `Source/TuiXiangZi/LevelData/LevelSerializer.cpp` | 新建 | 序列化器实现 |

### LevelData/LevelDataTypes.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "LevelDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FMechanismGroupStyleData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GroupId = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor BaseColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor ActiveColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
};

USTRUCT(BlueprintType)
struct FCellData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint GridPos = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CellType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName VisualStyleId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GroupId = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ExtraParam = 0;
};

USTRUCT(BlueprintType)
struct FLevelData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FCellData> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint PlayerStart = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FIntPoint> BoxPositions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMechanismGroupStyleData> GroupStyles;
};
```

### LevelData/LevelSerializer.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "LevelData/LevelDataTypes.h"
#include "LevelSerializer.generated.h"

UCLASS()
class TUIXIANGZI_API ULevelSerializer : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static bool SaveToJson(const FLevelData& Data, const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static bool LoadFromJson(const FString& FilePath, FLevelData& OutData);

    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static FString GetDefaultLevelDirectory();

    UFUNCTION(BlueprintCallable, Category = "LevelData")
    static void GetAvailableLevelFiles(TArray<FString>& OutFileNames);

private:
    static TSharedRef<FJsonValue> IntPointToJsonValue(FIntPoint Point);
    static bool JsonValueToIntPoint(const TSharedPtr<FJsonValue>& Value, FIntPoint& OutPoint);
    static TSharedRef<FJsonValue> ColorToJsonValue(FLinearColor Color);
    static bool JsonValueToColor(const TSharedPtr<FJsonValue>& Value, FLinearColor& OutColor);
    static TSharedRef<FJsonObject> CellDataToJson(const FCellData& Cell);
    static bool JsonToCellData(const TSharedPtr<FJsonObject>& JsonObj, FCellData& OutCell);
    static TSharedRef<FJsonObject> GroupStyleToJson(const FMechanismGroupStyleData& Style);
    static bool JsonToGroupStyle(const TSharedPtr<FJsonObject>& JsonObj, FMechanismGroupStyleData& OutStyle);
};
```

### LevelData/LevelSerializer.cpp 关键实现

手动 JSON 序列化（非 FJsonObjectConverter），原因：
- FIntPoint 需紧凑 `[x, y]` 格式
- 可选字段默认值时不写入 JSON
- 完整实现包括 SaveToJson / LoadFromJson 及所有辅助函数

JSON 格式示例：
```json
{
  "cells": [
    { "gridPos": [0, 0], "cellType": "Wall" },
    { "gridPos": [1, 1], "cellType": "Floor", "visualStyleId": "wood_floor" },
    { "gridPos": [2, 2], "cellType": "PressurePlate", "groupId": 0 }
  ],
  "playerStart": [1, 1],
  "boxes": [[3, 3], [4, 2]],
  "groupStyles": [
    { "groupId": 0, "displayName": "红门", "baseColor": [1, 0.1, 0.1], "activeColor": [1, 0.4, 0.4] }
  ]
}
```

### 1C 关键注意事项

1. **手动 JSON**: FIntPoint → `[x, y]`，可选字段默认值时省略
2. **UTF-8 无 BOM**: `ForceUTF8WithoutBOM` 确保跨平台兼容
3. **目录自动创建**: SaveToJson 在写入前检查并创建目录
4. **容错处理**: LoadFromJson 对每个字段使用 `TryGet*` 方法
5. **存储路径**: `FPaths::ProjectSavedDir() / TEXT("Levels/")`

### 1C 验收标准

1. 项目编译通过
2. SaveToJson 成功写入 JSON 文件
3. JSON 格式与架构文档示例一致
4. LoadFromJson 成功读取，数据与原始一致
5. 可选字段默认值时不出现在 JSON 中
6. 加载不存在/格式错误的文件返回 false 不崩溃
7. **往返测试**: Save → Load → Save，两次 JSON 完全一致

---

## 子任务间接口约定

| 接口关系 | 提供方 | 消费方 | 说明 |
|---------|--------|--------|------|
| `EGridCellType`, `FGridCell` | 1A | 1B | TMap Value 类型、可通行性判断 |
| `GridTypeUtils::StringToCellType()` | 1A | 1B | InitFromLevelData 中转换 |
| `FLevelData`, `FCellData` | 1C | 1B | InitFromLevelData 参数 |
| `DirectionToOffset()` | 1A | 1B (Phase 3) | TryMoveActor 中使用 |

**关键约定**：
- CellType 字符串编码：1A 定义映射，1C 的 JSON 使用字符串，1B 在 InitFromLevelData 中通过工具函数转换
- 默认值：`GroupId = -1` 无分组，`VisualStyleId = NAME_None` 默认样式，`ExtraParam = 0` 无扩展

---

## Phase 1 完整文件清单

| 子任务 | 文件路径 | 操作 |
|--------|----------|------|
| 1A | `Source/TuiXiangZi/TuiXiangZi.Build.cs` | 修改 |
| 1A | `Source/TuiXiangZi/Grid/GridTypes.h` | 新建 |
| 1A | `Source/TuiXiangZi/Grid/GridTypes.cpp` | 新建 |
| 1B | `Source/TuiXiangZi/Grid/GridManager.h` | 新建 |
| 1B | `Source/TuiXiangZi/Grid/GridManager.cpp` | 新建 |
| 1C | `Source/TuiXiangZi/LevelData/LevelDataTypes.h` | 新建 |
| 1C | `Source/TuiXiangZi/LevelData/LevelSerializer.h` | 新建 |
| 1C | `Source/TuiXiangZi/LevelData/LevelSerializer.cpp` | 新建 |

**总计**: 1 个修改 + 7 个新建 = 8 个文件

---

## Phase 1 整体验收标准

1. 在关卡中放置 AGridManager Actor
2. `InitEmptyGrid(8, 6)` → 场景中出现 8x6 地板网格
3. `SetCell(FIntPoint(0, 0), WallCell)` → 对应位置变为墙壁
4. `RemoveCell(FIntPoint(3, 3))` → 对应位置地板消失
5. 构造 FLevelData → `SaveToJson` 写入 JSON
6. `LoadFromJson` 读取得到相同数据
7. 将 FLevelData 传给 `InitFromLevelData` → 场景重建
8. 坐标转换双向正确：`WorldToGrid(GridToWorld(pos)) == pos`
