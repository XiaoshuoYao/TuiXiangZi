# CLAUDE.md — KuluoBishi (TuiXiangZi)

## Architecture Reference
**Before making ANY changes or doinf ANY plan, read [ARCHITECTURE.md](ARCHITECTURE.md)** to understand the project structure and subsystem relationships.

**After making changes that affect project structure, add/remove/rename files, add new subsystems, change data flows, or modify class relationships, update the relevant sections in ARCHITECTURE.md.**

## Project Basics
- Unreal Engine 5.5 Sokoban (推箱子) puzzle game with built-in level editor
- Single C++ module: `TuiXiangZi`
- Source code: `Source/TuiXiangZi/`
- Level files: JSON format in `Content/Levels/`

## Code Conventions
- Grid positions use `FIntPoint` throughout
- Mechanism components inherit from `UGridMechanismComponent`
- Visual actors inherit from `ATileActor`
- UI widgets follow UMG pattern with `BindWidget` properties
- Level data serialized as JSON via `ULevelSerializer`
- Movement animation uses `UTimelineComponent` (0.15s duration)
- Events use Unreal multicast delegates (`DECLARE_DYNAMIC_MULTICAST_DELEGATE`)

## Key Files
- `Grid/GridManager.h` — Central grid coordinator, start here for gameplay logic
- `Framework/SokobanGameMode.h` — Gameplay orchestration
- `Editor/LevelEditorGameMode.h` — Editor orchestration
- `LevelData/LevelDataTypes.h` — All data structures
- `LevelData/LevelSerializer.h` — JSON level format

## Debug
- You can add debug logs and prompt user to get the result back from UE editor

## Verification
- No need to build the project to verify, user will build manually