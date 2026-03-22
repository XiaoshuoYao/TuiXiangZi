#pragma once

#include "CoreMinimal.h"

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
	inline const FName Teleported         = TEXT("Mechanism.Teleported");
	inline const FName EditorBrushChanged = TEXT("Editor.BrushChanged");
	inline const FName EditorCellPainted  = TEXT("Editor.CellPainted");
	inline const FName EditorCellErased   = TEXT("Editor.CellErased");
	inline const FName EditorGroupCreated = TEXT("Editor.GroupCreated");
	inline const FName EditorModeChanged  = TEXT("Editor.ModeChanged");
	inline const FName EditorNewLevel     = TEXT("Editor.NewLevel");
	inline const FName EditorLevelSaved   = TEXT("Editor.LevelSaved");
	inline const FName EditorLevelLoaded  = TEXT("Editor.LevelLoaded");
	inline const FName EditorLevelTested  = TEXT("Editor.LevelTested");
	inline const FName EditorGridBoundsChanged = TEXT("Editor.GridBoundsChanged");
	inline const FName EditorTeleporterDirectionChanged = TEXT("Editor.TeleporterDirectionChanged");
}
