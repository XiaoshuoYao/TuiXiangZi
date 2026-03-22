# TuiXiangZi (推箱子) - Architecture Overview

Unreal Engine 5.5 Sokoban puzzle game with built-in level editor. Single module: `TuiXiangZi`.

## Directory Structure

```
Source/TuiXiangZi/
├── Events/          # GameEventBus (UWorldSubsystem), GameEventPayload, GameEventTags
├── Framework/       # GameInstance, GameMode, GameState, SaveGame
├── Grid/            # GridManager, GridTypes, TileStyleCatalog, TileActor
├── Gameplay/        # SokobanCharacter, PushableBoxComponent, GroupColorIndicatorComponent
│   ├── Mechanisms/  # GridMechanismComponent (base), Door, PressurePlate, Goal, Teleporter
│   └── Modifiers/   # TileModifierComponent (base), IceTileModifier
├── LevelData/       # LevelDataTypes, LevelSerializer (JSON)
├── Editor/          # LevelEditorGameMode, EditorOverlayManager, LevelEditorPawn, BrushTypes
│   └── Overlays/    # GridLineOverlay, CoordinateLabelOverlay, TeleporterArrowOverlay
├── Tutorial/        # TutorialTypes, TutorialDataAsset, TutorialSubsystem
└── UI/              # MainMenu, LevelSelect, PauseMenu, TutorialWidget, PlayerController
    └── Editor/      # EditorMain, Sidebar, Toolbar, StatusBar, GroupManager, Dialogs
```

## Core Subsystems

### 1. Events (`Events/`)
- **GameEventBus** (`UWorldSubsystem`) — Central event bus. All gameplay events (movement, tutorials, editor actions) are broadcast via `FName` tags and `FGameEventPayload`. Subscribers register with `Subscribe(Tag, Delegate)`. Synchronous dispatch, native `DECLARE_MULTICAST_DELEGATE`.
- **GameEventPayload** — Generic payload struct with Actor, GridPos, FromPos, ToPos, IntParam, FloatParam fields and factory methods (MakeActorMoved, MakeGridPos, MakeGridBounds, etc.).
- **GameEventTags** — Namespace with `inline const FName` constants for all event tags (e.g. `Grid.ActorMoved`, `Player.Moved`, `Editor.CellPainted`, `Editor.GridBoundsChanged`).

### 2. Grid System (`Grid/`)
- **GridManager** — Central coordinator. Stores cells as `TMap<FIntPoint, FGridCell>`, manages occupancy, validates movement, spawns visual actors. Broadcasts events via `UGameEventBus` (`Grid.ActorMoved`, `Grid.PlayerEnteredGoal`, `Grid.PitFilled`).
- **GridTypes** — Enums (`EGridCellType`, `EMoveDirection`) and structs (`FGridCell`).
- **TileStyleCatalog** — DataAsset for visual style variants per cell type.
- **TileActor** — Base actor for all grid-placed visual elements.

### 3. Framework (`Framework/`)
- **SokobanGameInstance** — Level selection state (Preset/Custom/Editor), progression tracking, level discovery.
- **SokobanGameMode** — Gameplay orchestration: level loading, win detection, box-on-plate visual updates.
- **SokobanGameState** — Runtime state: step counter, completion flag, undo snapshot stack (`TArray<FLevelSnapshot>`). Owns `CaptureSnapshot`, `PushSnapshot`, `PopSnapshot`, `IncrementSteps`.
- **SokobanSaveGame** — Persistence: completed levels, highest unlocked index.

### 4. Gameplay (`Gameplay/`)
- **SokobanCharacter** — Grid-based 4-direction movement, Enhanced Input (`Started` + `Completed`), CMC-driven walk animation (0.15s), hold-to-repeat continuous movement (chains next move on animation completion via `bHoldingDirection` flag), top-down camera.
- **PushableBoxComponent** — Box push/fall logic, plate feedback, material instance dynamics.
- **GroupColorIndicatorComponent** — Blueprintable group color display. BP subclasses override `OnUpdateVisual` for custom visuals. Auto-notified by GridManager and GridMechanismComponent.
- **Mechanisms** (component-based, attached to TileActor):
  - `GridMechanismComponent` (abstract base) — activation, passability, group roles, editor placement flow.
  - `DoorMechanismComponent` — blocks passage when closed, animated open/close, activated by pressure plate groups. Multiple doors can share one group.
  - `PressurePlateMechanismComponent` — triggers group activation when occupied by a box.
  - `GoalMechanismComponent` — win condition marker.
  - `TeleporterMechanismComponent` — paired one-to-one via group system, supports bidirectional/unidirectional (ExtraParam: 0=Bidirectional, 1=Entry, 2=Exit).

### 5. Level Data (`LevelData/`)
- **LevelDataTypes** — `FLevelData` (cells, player start, group styles), `FCellData`, `FBoxData`, `FMechanismGroupStyleData`.
- **LevelSerializer** — JSON serialization/deserialization, file discovery.

### 6. Editor (`Editor/`)
- **LevelEditorGameMode** — Brush system (10 brushes), editor modes, group management (door plates + teleporter pairs; supports multiple doors per group), validation, save/load/test. Broadcasts editor events via `UGameEventBus` (`Editor.*` tags). No direct overlay references — overlays are event-driven.
- **EditorOverlayManager** — Actor that owns overlay components. Manages overlay mode cycling (G key) and coordinated debug line flush/rebuild.
- **EditorOverlayComponent** (abstract base) — Event-driven overlay component. Subscribes to `UGameEventBus` events, manages visibility and dirty-state.
  - `GridLineOverlay` — grid lines via `DrawDebugLine`, subscribes to `Editor.GridBoundsChanged`.
  - `CoordinateLabelOverlay` — coordinate labels via `ATextRenderActor`, subscribes to `Editor.GridBoundsChanged`.
  - `TeleporterArrowOverlay` — teleporter connection arrows via `DrawDebugLine`, subscribes to cell change events, self-queries `GridManager`.
- **LevelEditorPawn** — Editor input handling, dialog management.

### 7. Tutorial (`Tutorial/`)
- **TutorialTypes** — Enums and structs for tutorial conditions/steps. 5 condition types: `None`, `Immediate`, `AfterSteps`, `OnGridPosition`, `OnGameplayEvent`. All game events matched via `OnGameplayEvent` + tag name.
- **TutorialDataAsset** — Per-level tutorial configs (gameplay + editor).
- **TutorialSubsystem** (`UWorldSubsystem`) — Tutorial flow management. Subscribes to all `UGameEventBus` events in `OnWorldBeginPlay`. Supports gameplay and editor contexts via unified `FTutorialCondition` model.

### 8. UI (`UI/`)
- **Menu**: MainMenuWidget (BindWidget buttons + WidgetSwitcher, NativeConstruct binding), PresetLevelSelectWidget (BindWidget ScrollBox/buttons, dynamically creates level entry widgets in C++, selection highlight via Border styling), CustomLevelSelectWidget (same pattern as Preset).
- **In-Game**: PauseMenuWidget (BindWidget buttons, NativeConstruct binding), TutorialWidget (BindWidget, NativeConstruct binding).
- **Editor**: EditorMainWidget, Sidebar, Toolbar, StatusBar, GroupManagerPanel, Dialogs (New/Load/Save/Confirm), ValidationResultPanel, ColorPickerPopup.
- **Pattern**: All menu/in-game widgets follow the same BindWidget + NativeConstruct pattern as editor widgets. Blueprint provides only layout skeleton; all logic, button binding, dynamic content creation, and styling are in C++.

## Key Data Flows

### Gameplay Move
```
Player Input (Started) → SokobanCharacter::OnMoveInput (set bHoldingDirection)
→ CaptureSnapshot (undo, before move) → GridManager::TryMoveActor
→ Validate (IsCellPassable, CanPushBoxTo) → Update occupancy → Broadcast Grid.ActorMoved
→ (if on Teleporter) CheckTeleporters → Broadcast Teleported → SnapToGridPos
→ Animate (CMC walk) → IncrementSteps → CheckGoalCondition
→ (if move failed) PopSnapshot (rollback)
→ Tick: on animation finish, if bHoldingDirection → chain OnMoveInput(HeldDirection)
→ Player Release (Completed) → bHoldingDirection = false
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
- **Component-Based Mechanisms** — Pluggable mechanism components on TileActor.
- **Descriptor-Driven Types** — `FCellTypeDescriptor` (GridTypes.h) centralizes cell type properties (passability, underlay, erase behavior, serialization string). `FBrushDescriptor` (EditorBrushTypes.h) centralizes editor brush properties (display name, shortcut, icon color, cell type mapping). Adding a new cell type = add enum value + add one row to each descriptor table.
- **Event-Driven** — Central `UGameEventBus` (`UWorldSubsystem`) with `FName`-tagged events for decoupling subsystems. Editor overlays subscribe to events independently — no direct coupling between GameMode and overlay components.
- **Undo/Snapshot** — Stack-based move history in GameState.
- **Style Catalog** — Centralized visual style management via DataAsset.
- **Editor/Gameplay Separation** — Distinct GameModes for each context.

## Dependencies
Core, CoreUObject, Engine, InputCore, EnhancedInput, Niagara, Json, JsonUtilities, ProceduralMeshComponent, UMG, Slate, SlateCore, RenderCore, NavigationSystem, AIModule.
