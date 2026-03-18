# Module G: 后端修改（GameMode + Pawn）

## 概述
修改现有的 LevelEditorGameMode 和 LevelEditorPawn，添加 UI 所需的接口、脏标记、快捷键、弹窗状态检查等。

## 前置依赖
- **编译依赖:** Module F（EditorMainWidget 头文件，Pawn 持有 MainWidget 指针）
- **必须在 Module F 完成后开发**
- 也可与 Module F 同时开发：先添加 GameMode 新接口（无依赖），再添加 Pawn 修改

## 修改文件
```
Source/TuiXiangZi/Editor/LevelEditorGameMode.h   (修改)
Source/TuiXiangZi/Editor/LevelEditorGameMode.cpp  (修改)
Source/TuiXiangZi/Editor/LevelEditorPawn.h        (修改)
Source/TuiXiangZi/Editor/LevelEditorPawn.cpp       (修改)
```

---

## Part 1: LevelEditorGameMode 修改

### 1.1 脏标记（Dirty Flag）

**目的：** UI 在新建/加载/返回时判断是否需要 "未保存确认" 弹窗。

```cpp
// .h 新增
protected:
    bool bIsDirty = false;

public:
    UFUNCTION(BlueprintCallable)
    bool IsDirty() const { return bIsDirty; }
```

**需要设置 bIsDirty = true 的位置：**
| 方法 | 位置 |
|------|------|
| PaintAtGrid() | 成功放置后 |
| EraseAtGrid() | 成功擦除后 |
| NewLevel() | 创建完成后（新关卡也算 "有内容"... 不，NewLevel 应该清除脏标记） |
| CreateNewGroup() | 创建分组后 |
| DeleteGroup() | 删除分组后 |
| SetGroupColor() | 修改颜色后 |

**需要设置 bIsDirty = false 的位置：**
| 方法 | 位置 |
|------|------|
| SaveLevel() | 保存成功后 |
| NewLevel() | 创建新关卡后（重置状态）|
| LoadLevel() | 加载成功后 |

### 1.2 VisualStyleId 支持

**目的：** Sidebar 的 Variant 选择需要 GameMode 存储当前选中的样式 ID，并在 PaintAtGrid 时写入 Cell。

```cpp
// .h 新增
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Editor")
    FName CurrentVisualStyleId = NAME_None;

public:
    UFUNCTION(BlueprintCallable, Category = "Editor")
    void SetCurrentVisualStyleId(FName NewStyleId);

    UFUNCTION(BlueprintCallable, Category = "Editor")
    FName GetCurrentVisualStyleId() const { return CurrentVisualStyleId; }

    // 提供 TileStyleCatalog 给 UI
    UFUNCTION(BlueprintCallable, Category = "Editor")
    UTileStyleCatalog* GetTileStyleCatalog() const;
```

```cpp
// .cpp
void ALevelEditorGameMode::SetCurrentVisualStyleId(FName NewStyleId)
{
    CurrentVisualStyleId = NewStyleId;
}

UTileStyleCatalog* ALevelEditorGameMode::GetTileStyleCatalog() const
{
    return GridManagerRef ? GridManagerRef->GetTileStyleCatalog() : nullptr;
}
```

**PaintAtGrid 修改：**
```cpp
// 在 PaintAtGrid() 中，创建 FGridCell 后添加:
Cell.VisualStyleId = CurrentVisualStyleId;
// 然后 GridManager->SetCell(Pos, Cell)
```

### 1.3 统计刷新 Delegate（可选）

当前 MainWidget 需要在每次 Paint/Erase 后手动调用 RefreshStats()。可选方案是添加一个 delegate：

```cpp
// 可选: 如果不想让 Pawn 每次都手动通知 MainWidget
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGridContentChanged);
UPROPERTY(BlueprintAssignable)
FOnGridContentChanged OnGridContentChanged;

// 在 PaintAtGrid/EraseAtGrid 末尾:
OnGridContentChanged.Broadcast();
```

---

## Part 2: LevelEditorPawn 修改

### 2.1 UI 创建

```cpp
// .h 新增
protected:
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UEditorMainWidget> MainWidgetClass;

    UPROPERTY()
    UEditorMainWidget* MainWidget = nullptr;
```

```cpp
// .cpp BeginPlay() 末尾添加:
void ALevelEditorPawn::BeginPlay()
{
    Super::BeginPlay();
    // ... 现有的 camera/input 初始化 ...

    // 创建 UI
    if (MainWidgetClass)
    {
        APlayerController* PC = Cast<APlayerController>(GetController());
        if (PC)
        {
            MainWidget = CreateWidget<UEditorMainWidget>(PC, MainWidgetClass);
            if (MainWidget)
            {
                MainWidget->AddToViewport(0);
            }
            // 设置输入模式: 游戏和UI并存
            FInputModeGameAndUI InputMode;
            InputMode.SetHideCursorDuringCapture(false);
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(true);
        }
    }
}
```

### 2.2 弹窗状态检查

在所有输入处理前检查弹窗状态：

```cpp
void ALevelEditorPawn::HandlePainting()
{
    // 新增: 弹窗打开时不处理
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    // ... 原有逻辑 ...
}

void ALevelEditorPawn::HandleErasing()
{
    // 新增: 弹窗打开时不处理
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    // ... 原有逻辑 ...
}

void ALevelEditorPawn::HandlePanning(float DeltaTime)
{
    // 新增: 弹窗打开时不处理
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    // ... 原有逻辑 ...
}
```

### 2.3 擦除确认

修改 HandleErasing() 添加确认判断：

```cpp
void ALevelEditorPawn::HandleErasing()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    FIntPoint GridPos;
    if (!RaycastToGrid(GridPos)) return;

    ALevelEditorGameMode* GM = Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    // 新增: 检查是否需要确认
    if (GM->ShouldConfirmErase(GridPos))
    {
        // 擦除确认只触发一次（松开右键后才能再次触发）
        if (!bEraseConfirmPending)
        {
            bEraseConfirmPending = true;
            if (MainWidget)
            {
                MainWidget->RequestEraseConfirm(GridPos);
            }
        }
        return;
    }

    // 原有逻辑: 直接擦除
    if (GridPos != LastErasedGridPos || !bHasLastErasedPos)
    {
        GM->EraseAtGrid(GridPos);
        LastErasedGridPos = GridPos;
        bHasLastErasedPos = true;
        if (MainWidget) MainWidget->RefreshStats();
    }
}
```

```cpp
// .h 新增
bool bEraseConfirmPending = false;

// OnRightClickCompleted 中重置:
void ALevelEditorPawn::OnRightClickCompleted(const FInputActionValue& Value)
{
    bIsErasing = false;
    bEraseConfirmPending = false;
    bHasLastErasedPos = false;
}
```

### 2.4 绘制后刷新统计

```cpp
void ALevelEditorPawn::HandlePainting()
{
    // ... 原有逻辑 ...
    if (GridPos != LastPaintedGridPos || !bHasLastPaintedPos)
    {
        GM->PaintAtGrid(GridPos);
        LastPaintedGridPos = GridPos;
        bHasLastPaintedPos = true;

        // 新增: 刷新 UI 统计
        if (MainWidget) MainWidget->RefreshStats();
    }
}
```

### 2.5 快捷键

**新增 InputAction 资产：**
```
Content/Input/Actions/IA_BrushSelect.uasset     — 数字键 1-8 + 0/E
Content/Input/Actions/IA_Shortcut_New.uasset     — Ctrl+N
Content/Input/Actions/IA_Shortcut_Save.uasset    — Ctrl+S
Content/Input/Actions/IA_Shortcut_Load.uasset    — Ctrl+O
Content/Input/Actions/IA_Shortcut_Test.uasset    — F5
Content/Input/Actions/IA_Shortcut_Esc.uasset     — Escape
```

**InputMappingContext 绑定：**
```
EditorMappingContext 中添加:
  1-8, 0 → IA_BrushSelect (使用 Modifiers 区分)
  E      → IA_BrushSelect (Eraser)
  Ctrl+N → IA_Shortcut_New
  Ctrl+S → IA_Shortcut_Save
  Ctrl+O → IA_Shortcut_Load
  F5     → IA_Shortcut_Test
  Escape → IA_Shortcut_Esc
```

**简化方案（推荐）：** 不使用 EnhancedInput 处理快捷键，改用 `APlayerController::InputComponent` 直接绑定按键：

```cpp
// .h 新增
void HandleKeyBrush1();  // ... HandleKeyBrush8(), HandleKeyBrushE()
void HandleShortcutNew();
void HandleShortcutSave();
void HandleShortcutLoad();
void HandleShortcutTest();
void HandleShortcutEsc();
```

```cpp
// SetupPlayerInputComponent 中:
void ALevelEditorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // ... 现有的 EnhancedInput 绑定 ...

    // 快捷键 (使用 Action binding)
    PlayerInputComponent->BindKey(EKeys::One,   IE_Pressed, this, &HandleKeyBrush<Floor>);
    PlayerInputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &HandleKeyBrush<Wall>);
    // ... 3-8 ...
    PlayerInputComponent->BindKey(EKeys::Zero,  IE_Pressed, this, &HandleKeyBrush<Eraser>);
    PlayerInputComponent->BindKey(EKeys::E,     IE_Pressed, this, &HandleKeyBrush<Eraser>);

    // Ctrl 组合键需要在 Action 中检测 modifier
    // 或使用 BindAction + InputChord
}
```

**Esc 处理：**
```cpp
void ALevelEditorPawn::HandleShortcutEsc()
{
    if (MainWidget)
    {
        MainWidget->HandleEscPressed();
        // HandleEscPressed 内部处理三级优先级
    }
}
```

**数字键笔刷选择：**
```cpp
void ALevelEditorPawn::HandleKeyBrush(EEditorBrush Brush)
{
    // 弹窗打开时忽略快捷键
    if (MainWidget && MainWidget->IsDialogOpen()) return;

    // PlacingPlatesForDoor 模式下忽略笔刷切换
    ALevelEditorGameMode* GM = Cast<ALevelEditorGameMode>(GetWorld()->GetAuthGameMode());
    if (GM && GM->GetEditorMode() != EEditorMode::Normal) return;

    if (MainWidget) MainWidget->RequestSetBrush(Brush);
}
```

**Ctrl 组合键：**
```cpp
void ALevelEditorPawn::HandleShortcutSave()
{
    if (MainWidget && MainWidget->IsDialogOpen()) return;
    if (MainWidget) MainWidget->RequestToolbarAction(EToolbarAction::Save);
}
// New/Load/Test 同理
```

---

## 修改汇总清单

### LevelEditorGameMode.h 新增
- `bool bIsDirty`
- `bool IsDirty() const`
- `FName CurrentVisualStyleId`
- `void SetCurrentVisualStyleId(FName)`
- `FName GetCurrentVisualStyleId() const`
- `UTileStyleCatalog* GetTileStyleCatalog() const`
- （可选）`FOnGridContentChanged OnGridContentChanged`

### LevelEditorGameMode.cpp 修改
- PaintAtGrid: 添加 `Cell.VisualStyleId = CurrentVisualStyleId` + `bIsDirty = true`
- EraseAtGrid: 添加 `bIsDirty = true`
- SaveLevel: 成功后 `bIsDirty = false`
- NewLevel: 完成后 `bIsDirty = false`
- LoadLevel: 成功后 `bIsDirty = false`
- CreateNewGroup/DeleteGroup/SetGroupColor: `bIsDirty = true`

### LevelEditorPawn.h 新增
- `TSubclassOf<UEditorMainWidget> MainWidgetClass`
- `UEditorMainWidget* MainWidget`
- `bool bEraseConfirmPending`
- 快捷键处理方法声明

### LevelEditorPawn.cpp 修改
- BeginPlay: 创建 MainWidget + AddToViewport + 设置输入模式
- HandlePainting: 弹窗检查 + RefreshStats
- HandleErasing: 弹窗检查 + 擦除确认逻辑
- HandlePanning: 弹窗检查
- OnRightClickCompleted: 重置 bEraseConfirmPending
- SetupPlayerInputComponent: 绑定快捷键
- 新增快捷键处理方法实现

---

## 可并行的子任务

本模块内部可拆分为 2 个可并行的部分：

**G1: GameMode 修改**（无依赖，可与 A~E 并行）
- 脏标记
- VisualStyleId 支持
- PaintAtGrid 修改

**G2: Pawn 修改**（依赖 Module F 的 EditorMainWidget 头文件）
- UI 创建
- 弹窗状态检查
- 擦除确认
- 快捷键

---

## 验收标准
1. IsDirty: 绘制/擦除后 = true; 保存/新建/加载后 = false
2. VisualStyleId: 设置后 PaintAtGrid 创建的 Cell 包含正确的 StyleId
3. UI 创建: 进入编辑器关卡后 MainWidget 自动显示
4. 输入模式: 鼠标光标可见，可同时操作 UI 和 Viewport
5. 弹窗屏蔽: 弹窗打开时 Viewport 绘制/擦除/平移全部无效
6. 擦除确认: 右键点击门格子弹出确认框; 确认后擦除; 取消后不擦除
7. 快捷键 1-8/E: 正确切换笔刷（PlacingPlatesForDoor 模式下忽略）
8. Ctrl+N/S/O, F5: 触发对应 Toolbar 功能
9. Esc: 三级优先级正确
