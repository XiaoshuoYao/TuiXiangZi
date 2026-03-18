# 编辑器 UI 实现总计划（主 Agent 用）

## 概述
关卡编辑器 UI 系统，共 11 个 Widget + 2 个后端修改，拆分为 7 个模块。

## 模块依赖图

```
 并行层 1（无依赖，5 个模块可同时开发）
 ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
 │ Module A │ │ Module B │ │ Module C │ │ Module D │ │ Module E │ │Module G1 │
 │ Dialogs  │ │StatusBar │ │ Sidebar  │ │ Toolbar  │ │ Group    │ │GM修改    │
 │          │ │          │ │          │ │          │ │ System   │ │(脏标记+  │
 │ 3 Widget │ │ 1 Widget │ │ 1 Widget │ │ 1 Widget │ │ 3 Widget │ │StyleId)  │
 └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘ └──────────┘
      │            │            │            │            │
      └────────────┴────────────┴────────────┴────────────┘
                                │
                    ┌───────────┴───────────┐
 并行层 2          │      Module F         │
                    │   EditorMainWidget    │
                    │   (集成 + 事件路由)     │
                    └───────────┬───────────┘
                                │
                    ┌───────────┴───────────┐
 并行层 3          │      Module G2        │
                    │   Pawn 修改           │
                    │ (UI创建+快捷键+擦除)   │
                    └───────────────────────┘
```

## 模块清单

| 模块 | 文件 | Widget 数 | 可并行 | 详细计划 |
|------|------|-----------|--------|---------|
| **A** Dialogs | ConfirmDialog, NewLevelDialog, ValidationResultPanel | 3 | 层1 ✅ | [EditorUI_Module_A_Dialogs.md](EditorUI_Module_A_Dialogs.md) |
| **B** StatusBar | EditorStatusBar | 1 | 层1 ✅ | [EditorUI_Module_B_StatusBar.md](EditorUI_Module_B_StatusBar.md) |
| **C** Sidebar | EditorSidebarWidget (含 Variant) | 1 | 层1 ✅ | [EditorUI_Module_C_Sidebar.md](EditorUI_Module_C_Sidebar.md) |
| **D** Toolbar | EditorToolbarWidget | 1 | 层1 ✅ | [EditorUI_Module_D_Toolbar.md](EditorUI_Module_D_Toolbar.md) |
| **E** Group System | GroupManagerPanel, GroupEntryWidget, ColorPickerPopup | 3 | 层1 ✅ | [EditorUI_Module_E_GroupSystem.md](EditorUI_Module_E_GroupSystem.md) |
| **F** MainWidget | EditorMainWidget (集成) | 1 | 层2 (依赖 A~E) | [EditorUI_Module_F_MainWidget.md](EditorUI_Module_F_MainWidget.md) |
| **G** Backend | GameMode 修改 + Pawn 修改 | 0 | G1=层1, G2=层3 | [EditorUI_Module_G_BackendChanges.md](EditorUI_Module_G_BackendChanges.md) |

**总计: 10 个 C++ Widget 类 + 2 个修改文件 + 10 个 Blueprint (WBP_)**

---

## 执行计划

### Phase 1: 并行开发（6 个 subagent 同时）

启动 6 个 subagent 并行执行：

| Subagent | 模块 | 任务描述 | 预计文件数 |
|----------|------|---------|-----------|
| agent-A | Module A | 实现 ConfirmDialog + NewLevelDialog + ValidationResultPanel | 6 (.h/.cpp) |
| agent-B | Module B | 实现 EditorStatusBar | 2 |
| agent-C | Module C | 实现 EditorSidebarWidget（含 Variant 子面板） | 2 |
| agent-D | Module D | 实现 EditorToolbarWidget | 2 |
| agent-E | Module E | 实现 GroupManagerPanel + GroupEntryWidget + ColorPickerPopup | 6 |
| agent-G1 | Module G (Part 1) | 修改 GameMode: 添加脏标记 + VisualStyleId + PaintAtGrid 修改 | 2 (修改) |

**并行规则：**
- 每个 subagent 只写自己模块的 .h/.cpp 文件
- 子面板只暴露 delegate + Refresh 方法，不引用其他子面板的头文件
- 所有子面板 #include 仅限 UE 核心头文件 + 自己的类型（EEditorBrush, EEditorMode, FMechanismGroupStyleData）
- 共享的枚举/结构体在 `EditorBrushTypes.h` 和 `LevelDataTypes.h` 中，已存在

### Phase 2: 集成（1 个 subagent）

等待 Phase 1 全部完成后：

| Subagent | 模块 | 任务描述 |
|----------|------|---------|
| agent-F | Module F | 实现 EditorMainWidget: 持有所有子面板, 绑定 delegate, 管理弹窗, 编排流程 |

### Phase 3: Pawn 接入（1 个 subagent）

等待 Phase 2 完成后：

| Subagent | 模块 | 任务描述 |
|----------|------|---------|
| agent-G2 | Module G (Part 2) | 修改 Pawn: UI 创建 + 弹窗检查 + 擦除确认 + 快捷键 |

### Phase 4: Blueprint 创建 + 集成测试

所有 C++ 完成后，需要手动（或通过 UE 编辑器）创建对应的 Blueprint Widget (WBP_)，配置 BindWidget 绑定。

---

## 共享类型引用

所有模块共享以下已存在的头文件（**不需要修改**）：

| 头文件 | 提供的类型 |
|--------|-----------|
| `Editor/EditorBrushTypes.h` | `EEditorBrush`, `EEditorMode` |
| `LevelData/LevelDataTypes.h` | `FMechanismGroupStyleData`, `FCellData`, `FLevelData` |
| `Grid/GridTypes.h` | `EGridCellType`, `FGridCell` |
| `Grid/TileStyleCatalog.h` | `UTileStyleCatalog`, `FTileVisualStyle` |
| `Editor/LevelEditorGameMode.h` | `ALevelEditorGameMode`, `FLevelValidationResult` |

**新增的共享类型（在 Module A 中定义）：**
```cpp
// 放在 EditorBrushTypes.h 或单独的 EditorUITypes.h 中
UENUM(BlueprintType)
enum class EValidationContext : uint8 { Save, Test };

UENUM(BlueprintType)
enum class EToolbarAction : uint8 { New, Save, Load, Test, Back };
```

---

## 文件目录结构（最终）

```
Source/TuiXiangZi/UI/Editor/
├─ EditorMainWidget.h/.cpp          (Module F)
├─ EditorSidebarWidget.h/.cpp       (Module C)
├─ EditorToolbarWidget.h/.cpp       (Module D)
├─ EditorStatusBar.h/.cpp           (Module B)
├─ GroupManagerPanel.h/.cpp          (Module E)
├─ GroupEntryWidget.h/.cpp           (Module E)
├─ ColorPickerPopup.h/.cpp           (Module E)
├─ ConfirmDialog.h/.cpp              (Module A)
├─ NewLevelDialog.h/.cpp             (Module A)
└─ ValidationResultPanel.h/.cpp      (Module A)

Source/TuiXiangZi/Editor/
├─ LevelEditorGameMode.h/.cpp       (Module G - 修改)
├─ LevelEditorPawn.h/.cpp            (Module G - 修改)
├─ EditorBrushTypes.h                (已存在, 可能新增 enum)
└─ ...

Content/Blueprints/UI/Editor/
├─ WBP_EditorMainWidget.uasset
├─ WBP_EditorSidebar.uasset
├─ WBP_EditorToolbar.uasset
├─ WBP_EditorStatusBar.uasset
├─ WBP_GroupManagerPanel.uasset
├─ WBP_GroupEntryWidget.uasset
├─ WBP_ColorPickerPopup.uasset
├─ WBP_ConfirmDialog.uasset
├─ WBP_NewLevelDialog.uasset
└─ WBP_ValidationResultPanel.uasset
```

---

## Subagent 启动模板

### Phase 1 启动命令（一次性启动 6 个）

每个 subagent 的 prompt 模板：
```
你是一个 UE5 C++ 开发者。请根据以下计划实现代码。

项目路径: f:/SideProject/KuluoBishi
计划文件: Content/AIPlan/EditorUI_Module_{X}_{Name}.md

请仔细阅读计划文件，然后:
1. 阅读计划中提到的已有代码（头文件/类型定义）
2. 创建计划中列出的 .h 和 .cpp 文件
3. 严格按照计划中的类定义、接口、交互逻辑实现
4. 不要修改其他模块的文件
5. 确保编译通过（仅依赖 UE 核心 + 共享类型头文件）
```

### Phase 2/3 启动
```
等待前置 Phase 完成后，用相同模板启动 Module F / Module G2。
Module F 需要 #include 所有 Phase 1 的头文件。
Module G2 需要 #include Module F 的头文件。
```

---

## 注意事项

1. **Blueprint 不由 subagent 创建** — .uasset 必须在 UE 编辑器中手工创建，C++ 中使用 `meta = (BindWidget)` 声明绑定点
2. **SV 平面材质** — ColorPickerPopup 的 SV 渐变建议用 Material Instance Dynamic，避免每帧生成纹理
3. **TFunction 与 Dynamic Delegate 不兼容** — Module F 中 ShowConfirmDialog 的 lambda 回调需要用 `PendingConfirmAction` 成员变量中转
4. **输入模式** — Pawn 必须设置 `FInputModeGameAndUI`，确保鼠标可同时操作 UI 和 Viewport
5. **点击穿透** — 中央 Viewport 区域不放任何 Widget，让鼠标事件自然穿透到 Pawn 的 RaycastToGrid

---

## 原始完整计划参考

详细的交互逻辑设计见 [PhaseUI_Editor.md](PhaseUI_Editor.md)（交互逻辑细化版，包含所有流程图和边界情况）。
