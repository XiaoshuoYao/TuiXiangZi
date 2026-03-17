# Phase 2: Core Actors — 详细实施计划

## 概述

Phase 2 包含三个可并行子任务，均依赖 Phase 1。核心目标：实现游戏中的可交互 Actor（角色、箱子）及全局视觉样式系统。

## 目录结构

```
Source/TuiXiangZi/
├── Grid/
│   ├── TileStyleCatalog.h           # 2C
│   └── TileStyleCatalog.cpp         # 2C
├── Gameplay/
│   ├── SokobanCharacter.h           # 2A
│   ├── SokobanCharacter.cpp         # 2A
│   ├── PushableBox.h                # 2B
│   └── PushableBox.cpp              # 2B
```

---

## 2A: SokobanCharacter

### 文件清单
| 文件 | 说明 |
|------|------|
| `Source/TuiXiangZi/Gameplay/SokobanCharacter.h/.cpp` | 角色类 |
| `Content/Input/IA_MoveUp/Down/Left/Right.uasset` | Enhanced Input Actions |
| `Content/Input/IMC_Sokoban.uasset` | Input Mapping Context |

### 核心签名

```cpp
UCLASS(Blueprintable)
class TUIXIANGZI_API ASokobanCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ASokobanCharacter();
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    FIntPoint CurrentGridPos;
    bool bIsMoving = false;
    float MoveDuration = 0.15f;
    UCurveFloat* MoveCurve;

    void SnapToGridPos(FIntPoint GridPos);

protected:
    UInputMappingContext* SokobanMappingContext;
    UInputAction* MoveUpAction / MoveDownAction / MoveLeftAction / MoveRightAction;

    void OnMoveUp/Down/Left/Right(const FInputActionValue& Value);
    void OnMoveInput(EMoveDirection Dir);

    UTimelineComponent* MoveTimeline;
    FVector MoveStartLocation, MoveTargetLocation;
    void SmoothMoveTo(FVector TargetWorldPos);
    void OnMoveTimelineUpdate(float Alpha);    // UFUNCTION
    void OnMoveTimelineFinished();              // UFUNCTION
    void OnMoveCompleted();
    void OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To);

    USpringArmComponent* CameraBoom;
    UCameraComponent* TopDownCamera;
    AGridManager* GridManagerRef;
};
```

### 关键实现要点
- Enhanced Input: 绑定 `ETriggerEvent::Started`（按下瞬间一次，非持续）
- CharacterMovement: `MaxWalkSpeed = 0.0f`，禁用自由移动，仅 `SetActorLocation` 控制位置
- FTimeline: MoveCurve 为 0→1 EaseInOut 曲线，通过 Lerp 插值平滑移动
- 移动流程: OnMoveInput → GridManager->TryMoveActor → 成功时广播 OnActorLogicalMoved → 角色 SmoothMoveTo
- 移动锁: bIsMoving 防止动画期间重复输入
- 相机: SpringArm 俯视角 (-75度)，TargetArmLength=1200

### Phase 1 接口依赖
- `EMoveDirection`, `AGridManager::TryMoveActor`, `AGridManager::GridToWorld`, `FOnActorLogicalMoved`

### 验收标准
1. WASD 四方向输入正确识别，每次按键触发一次移动
2. 移动锁正常工作
3. EaseInOut 平滑移动约 0.15 秒
4. 移动完成后精确对齐格子中心
5. SnapToGridPos 无动画瞬移
6. 完全事件驱动（依赖 GridManager 广播）

---

## 2B: PushableBox

### 文件清单
| 文件 | 说明 |
|------|------|
| `Source/TuiXiangZi/Gameplay/PushableBox.h/.cpp` | 箱子类 |

### 核心签名

```cpp
UCLASS(Blueprintable)
class TUIXIANGZI_API APushableBox : public AActor
{
    GENERATED_BODY()
public:
    APushableBox();
    virtual void BeginPlay() override;

    FIntPoint CurrentGridPos;
    UStaticMeshComponent* MeshComp;    // Cube, Scale 0.9
    UMaterialInstanceDynamic* DynamicMaterialInst;

    bool bIsMoving = false;
    float MoveDuration = 0.15f;
    UCurveFloat* MoveCurve;

    void SnapToGridPos(FIntPoint GridPos);
    void SmoothMoveTo(FVector TargetWorldPos);
    void PlayFallIntoHoleAnim();        // Phase 3 预留

protected:
    UTimelineComponent* MoveTimeline;
    void OnMoveTimelineUpdate(float Alpha);
    void OnMoveTimelineFinished();
    void OnMoveCompleted();
    void OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To);
    AGridManager* GridManagerRef;
};
```

### 关键实现要点
- Cube Mesh 缩放 0.9 留缝隙，碰撞设为 NoCollision
- 动态材质实例在 BeginPlay 创建（后续 Phase 5 变色用）
- 平滑移动与 SokobanCharacter 共用模式（各自独立实现，不提取组件）
- 只响应自身的 OnActorLogicalMoved 事件

### 验收标准
1. Cube 正确渲染，尺寸匹配格子
2. SnapToGridPos / SmoothMoveTo 工作正常
3. 动态材质实例创建成功
4. 无物理碰撞

---

## 2C: TileStyleCatalog

### 文件清单
| 文件 | 说明 |
|------|------|
| `Source/TuiXiangZi/Grid/TileStyleCatalog.h/.cpp` | DataAsset 类 |
| `Content/Data/DA_DefaultTileStyles.uasset` | 默认样式目录 |

### 核心签名

```cpp
USTRUCT(BlueprintType)
struct FTileVisualStyle
{
    GENERATED_BODY()
    FName StyleId;
    FString DisplayName;
    EGridCellType ApplicableType = EGridCellType::Floor;
    UStaticMesh* Mesh = nullptr;
    UMaterialInterface* Material = nullptr;
    UTextureRenderTarget2D* Thumbnail = nullptr;  // 自动渲染，无需手动设置
};

UCLASS(BlueprintType)
class TUIXIANGZI_API UTileStyleCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    TArray<FTileVisualStyle> Styles;
    const FTileVisualStyle* FindStyle(FName StyleId) const;
    TArray<const FTileVisualStyle*> GetStylesForType(EGridCellType Type) const;
    bool HasStyle(FName StyleId) const;
};
```

### GridManager 集成
- GridManager 新增成员 `UTileStyleCatalog* TileStyleCatalog`
- `ResolveTileVisual(FGridCell)` 查找样式，找不到 fallback 到默认 Mesh/Material

### 验收标准
1. 编辑器中可创建 UTileStyleCatalog DataAsset
2. FindStyle / GetStylesForType 正确工作
3. 编辑器中存在重复 StyleId 时输出警告

---

## 共享资源
- `Content/Data/Curve_EaseInOut.uasset`: SokobanCharacter 和 PushableBox 共用的 CurveFloat
- Phase 2 不需要新增 Build.cs 模块依赖
