# TuiXiangZi (推箱子) - Architecture Overview

Unreal Engine 5.5 Sokoban puzzle game with built-in level editor. Single module: `TuiXiangZi`.

## Directory Structure

```
Source/TuiXiangZi/
├── Framework/       # GameInstance, GameMode, GameState, SaveGame
├── Grid/            # GridManager, GridTypes, TileStyleCatalog, TileVisualActor
├── Gameplay/        # SokobanCharacter, PushableBoxComponent
│   └── Mechanisms/  # GridMechanismComponent (base), Door, PressurePlate, Goal
├── LevelData/       # LevelDataTypes, LevelSerializer (JSON)
├── Editor/          # LevelEditorGameMode, EditorGridVisualizer, LevelEditorPawn, BrushTypes
└── UI/              # MainMenu, LevelSelect, PauseMenu, PlayerController
    └── Editor/      # EditorMain, Sidebar, Toolbar, StatusBar, GroupManager, Dialogs
```

## Core Subsystems

### 1. Grid System (`Grid/`)
- **GridManager** — Central coordinator. Stores cells as `TMap<FIntPoint, FGridCell>`, manages occupancy, validates movement, spawns visual actors, broadcasts events (`OnActorLogicalMoved`, `OnPlayerEnteredGoal`, `OnPitFilled`).
- **GridTypes** — Enums (`EGridCellType`, `EMoveDirection`) and structs (`FGridCell`).
- **TileStyleCatalog** — DataAsset for visual style variants per cell type.
- **TileVisualActor** — Base actor for all grid-placed visual elements.

### 2. Framework (`Framework/`)
- **SokobanGameInstance** — Level selection state (Preset/Custom/Editor), progression tracking, level discovery.
- **SokobanGameMode** — Gameplay orchestration: level loading, undo, step counting, win detection.
- **SokobanGameState** — Runtime state: step counter, completion flag, undo snapshot stack (`TArray<FLevelSnapshot>`).
- **SokobanSaveGame** — Persistence: completed levels, highest unlocked index.

### 3. Gameplay (`Gameplay/`)
- **SokobanCharacter** — Grid-based 4-direction movement, Enhanced Input, timeline animation (0.15s), top-down camera.
- **PushableBoxComponent** — Box push/fall logic, plate feedback, material instance dynamics.
- **Mechanisms** (component-based, attached to TileVisualActor):
  - `GridMechanismComponent` (abstract base) — activation, passability, group roles, editor placement flow.
  - `DoorMechanismComponent` — blocks passage when closed, animated open/close, activated by pressure plate groups.
  - `PressurePlateMechanismComponent` — triggers group activation when occupied by a box.
  - `GoalMechanismComponent` — win condition marker.

### 4. Level Data (`LevelData/`)
- **LevelDataTypes** — `FLevelData` (cells, player start, group styles), `FCellData`, `FBoxData`, `FMechanismGroupStyleData`.
- **LevelSerializer** — JSON serialization/deserialization, file discovery.

### 5. Editor (`Editor/`)
- **LevelEditorGameMode** — Brush system (9 brushes), editor modes (Normal, PlacingPlatesForDoor, EditingDoorGroup), group management, validation, save/load/test.
- **EditorGridVisualizer** — Procedural mesh grid lines.
- **LevelEditorPawn** — Editor input handling, dialog management.

### 6. UI (`UI/`)
- **Menu**: MainMenuWidget, PresetLevelSelectWidget, CustomLevelSelectWidget.
- **In-Game**: PauseMenuWidget, TuiXiangZiPlayerController.
- **Editor**: EditorMainWidget, EditorSidebarWidget, EditorToolbarWidget, EditorStatusBar, GroupManagerPanel, NewLevelDialog, LoadLevelDialog, SaveLevelDialog, ConfirmDialog, ValidationResultPanel, ColorPickerPopup.

## Key Data Flows

### Gameplay Move
```
Player Input → SokobanCharacter::OnMoveInput → GridManager::TryMoveActor
→ Validate (IsCellPassable, CanPushBoxTo) → Update occupancy → Broadcast OnActorLogicalMoved
→ Animate (Timeline) → CaptureSnapshot (undo) → IncrementSteps → CheckGoalCondition
```

### Level Load
```
SokobanGameMode::LoadLevel(JsonPath) → LevelSerializer::LoadFromJson
→ GridManager::InitFromLevelData → Spawn visual actors + mechanisms → Set player start
```

### Editor Save
```
LevelEditorGameMode::SaveLevel → Build FLevelData → Validate → LevelSerializer::SaveToJson
```

## Design Patterns
- **Grid-Based Discrete Movement** — All positions are `FIntPoint`.
- **Component-Based Mechanisms** — Pluggable mechanism components on TileVisualActor.
- **Descriptor-Driven Types** — `FCellTypeDescriptor` (GridTypes.h) centralizes cell type properties (passability, underlay, erase behavior, serialization string). `FBrushDescriptor` (EditorBrushTypes.h) centralizes editor brush properties (display name, shortcut, icon color, cell type mapping). Adding a new cell type = add enum value + add one row to each descriptor table.
- **Event-Driven** — Multicast delegates for decoupling subsystems.
- **Undo/Snapshot** — Stack-based move history in GameState.
- **Style Catalog** — Centralized visual style management via DataAsset.
- **Editor/Gameplay Separation** — Distinct GameModes for each context.

## Dependencies
Core, CoreUObject, Engine, InputCore, EnhancedInput, Niagara, Json, JsonUtilities, ProceduralMeshComponent, UMG, Slate, SlateCore, RenderCore, NavigationSystem, AIModule.
