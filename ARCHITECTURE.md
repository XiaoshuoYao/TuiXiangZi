# TuiXiangZi (推箱子) - Architecture Overview

Unreal Engine 5.5 Sokoban puzzle game with built-in level editor. Single module: `TuiXiangZi`.

## Directory Structure

```
Source/TuiXiangZi/
├── Events/          # GameEventBus (UWorldSubsystem), GameEventPayload, GameEventTags
├── Framework/       # GameInstance, GameMode, GameState, SaveGame
├── Grid/            # GridManager, GridTypes, TileStyleCatalog, TileVisualActor
├── Gameplay/        # SokobanCharacter, PushableBoxComponent, GroupColorIndicatorComponent
│   └── Mechanisms/  # GridMechanismComponent (base), Door, PressurePlate, Goal
├── LevelData/       # LevelDataTypes, LevelSerializer (JSON)
├── Editor/          # LevelEditorGameMode, EditorGridVisualizer, LevelEditorPawn, BrushTypes
├── Tutorial/        # TutorialTypes, TutorialDataAsset, TutorialSubsystem
└── UI/              # MainMenu, LevelSelect, PauseMenu, TutorialWidget, PlayerController
    └── Editor/      # EditorMain, Sidebar, Toolbar, StatusBar, GroupManager, Dialogs
```

## Core Subsystems

### 1. Events (`Events/`)
- **GameEventBus** (`UWorldSubsystem`) — Central event bus. All gameplay events (movement, tutorials, editor actions) are broadcast via `FName` tags and `FGameEventPayload`. Subscribers register with `Subscribe(Tag, Delegate)`. Synchronous dispatch, native `DECLARE_MULTICAST_DELEGATE`.
- **GameEventPayload** — Generic payload struct with Actor, GridPos, FromPos, ToPos, IntParam fields and factory methods.
- **GameEventTags** — Namespace with `inline const FName` constants for all event tags (e.g. `Grid.ActorMoved`, `Player.Moved`, `Editor.CellPainted`).

### 2. Grid System (`Grid/`)
- **GridManager** — Central coordinator. Stores cells as `TMap<FIntPoint, FGridCell>`, manages occupancy, validates movement, spawns visual actors. Broadcasts events via `UGameEventBus` (`Grid.ActorMoved`, `Grid.PlayerEnteredGoal`, `Grid.PitFilled`).
- **GridTypes** — Enums (`EGridCellType`, `EMoveDirection`) and structs (`FGridCell`).
- **TileStyleCatalog** — DataAsset for visual style variants per cell type.
- **TileVisualActor** — Base actor for all grid-placed visual elements.

### 3. Framework (`Framework/`)
- **SokobanGameInstance** — Level selection state (Preset/Custom/Editor), progression tracking, level discovery.
- **SokobanGameMode** — Gameplay orchestration: level loading, undo, step counting, win detection.
- **SokobanGameState** — Runtime state: step counter, completion flag, undo snapshot stack (`TArray<FLevelSnapshot>`).
- **SokobanSaveGame** — Persistence: completed levels, highest unlocked index.

### 4. Gameplay (`Gameplay/`)
- **SokobanCharacter** — Grid-based 4-direction movement, Enhanced Input, timeline animation (0.15s), top-down camera.
- **PushableBoxComponent** — Box push/fall logic, plate feedback, material instance dynamics.
- **GroupColorIndicatorComponent** — Blueprintable group color display. BP subclasses override `OnUpdateVisual` for custom visuals. Auto-notified by GridManager and GridMechanismComponent.
- **Mechanisms** (component-based, attached to TileVisualActor):
  - `GridMechanismComponent` (abstract base) — activation, passability, group roles, editor placement flow.
  - `DoorMechanismComponent` — blocks passage when closed, animated open/close, activated by pressure plate groups.
  - `PressurePlateMechanismComponent` — triggers group activation when occupied by a box.
  - `GoalMechanismComponent` — win condition marker.
  - `TeleporterMechanismComponent` — paired one-to-one via group system, supports bidirectional/unidirectional (ExtraParam: 0=Bidirectional, 1=Entry, 2=Exit).

### 5. Level Data (`LevelData/`)
- **LevelDataTypes** — `FLevelData` (cells, player start, group styles), `FCellData`, `FBoxData`, `FMechanismGroupStyleData`.
- **LevelSerializer** — JSON serialization/deserialization, file discovery.

### 6. Editor (`Editor/`)
- **LevelEditorGameMode** — Brush system (10 brushes), editor modes, group management (door plates + teleporter pairs), validation, save/load/test. Broadcasts editor events via `UGameEventBus` (`Editor.*` tags).
- **EditorGridVisualizer** — Procedural mesh grid lines.
- **LevelEditorPawn** — Editor input handling, dialog management.

### 7. Tutorial (`Tutorial/`)
- **TutorialTypes** — Enums and structs for tutorial conditions/steps.
- **TutorialDataAsset** — Per-level tutorial configs (gameplay + editor).
- **TutorialSubsystem** (`UWorldSubsystem`) — Tutorial flow management. Subscribes to `UGameEventBus` events in `OnWorldBeginPlay`. Supports gameplay and editor contexts via unified `FTutorialCondition` model.

### 8. UI (`UI/`)
- **Menu**: MainMenuWidget, PresetLevelSelectWidget, CustomLevelSelectWidget.
- **In-Game**: PauseMenuWidget, TutorialWidget, TuiXiangZiPlayerController.
- **Editor**: EditorMainWidget, Sidebar, Toolbar, StatusBar, GroupManagerPanel, Dialogs (New/Load/Save/Confirm), ValidationResultPanel, ColorPickerPopup.

## Key Data Flows

### Gameplay Move
```
Player Input → SokobanCharacter::OnMoveInput → GridManager::TryMoveActor
→ Validate (IsCellPassable, CanPushBoxTo) → Update occupancy → Broadcast OnActorLogicalMoved
→ Animate (Timeline) → CaptureSnapshot (undo) → IncrementSteps → CheckGoalCondition
→ (if on Teleporter) CheckTeleporters → Broadcast Teleported → SnapToGridPos
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
- **Event-Driven** — Central `UGameEventBus` (`UWorldSubsystem`) with `FName`-tagged events for decoupling subsystems. `BlueprintAssignable` delegates retained as thin wrappers for Blueprint UI bindings.
- **Undo/Snapshot** — Stack-based move history in GameState.
- **Style Catalog** — Centralized visual style management via DataAsset.
- **Editor/Gameplay Separation** — Distinct GameModes for each context.

## Dependencies
Core, CoreUObject, Engine, InputCore, EnhancedInput, Niagara, Json, JsonUtilities, ProceduralMeshComponent, UMG, Slate, SlateCore, RenderCore, NavigationSystem, AIModule.
