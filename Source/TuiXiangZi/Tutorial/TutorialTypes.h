#pragma once

#include "CoreMinimal.h"
#include "TutorialTypes.generated.h"

class UTutorialWidget;

// Unified condition type — used for both trigger and completion
UENUM(BlueprintType)
enum class ETutorialConditionType : uint8
{
	// --- General ---
	None,            // Completion: manual click to advance. Trigger: should not be used.
	Immediate,       // Trigger: LevelStart / AfterPrevious. Completion: not used.

	// --- Parameterized ---
	AfterSteps,      // After N player steps
	OnGridPosition,  // Player reaches a specific grid position
	OnGameplayEvent, // Named gameplay event (generic extension point)

	// --- Action-based ---
	OnPlayerMove,    // Player successfully moved
	OnPushBox,       // Player pushed a box
	OnUndo,          // Player performed undo
	OnReset,         // Player reset the level
	OnDoorOpened,    // A door was opened
};

// Unified condition config
USTRUCT(BlueprintType)
struct FTutorialCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tutorial")
	ETutorialConditionType Type = ETutorialConditionType::None;

	UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (EditCondition = "Type==ETutorialConditionType::AfterSteps"))
	int32 StepCount = 0;

	UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (EditCondition = "Type==ETutorialConditionType::OnGridPosition"))
	FIntPoint GridPos;

	UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (EditCondition = "Type==ETutorialConditionType::OnGameplayEvent"))
	FName EventTag;
};

// Display type (extensible for future)
UENUM(BlueprintType)
enum class ETutorialDisplayType : uint8
{
	TextPopup, // Text popup (current implementation)
	// MediaPopup, // Future: video/animation popup
	// Highlight,  // Future: highlight a grid cell
};

// Single tutorial step
USTRUCT(BlueprintType)
struct FTutorialStep
{
	GENERATED_BODY()

	// --- Display config ---
	UPROPERTY(EditAnywhere, Category = "Tutorial|Display")
	ETutorialDisplayType DisplayType = ETutorialDisplayType::TextPopup;

	UPROPERTY(EditAnywhere, Category = "Tutorial|Display")
	FText TutorialText;

	// Widget class override (empty = use Subsystem default)
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Tutorial|Display")
	TSubclassOf<UTutorialWidget> WidgetClassOverride;

	// --- Trigger condition (when to show this step) ---
	// Default: Immediate (show right away / after previous)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Trigger")
	FTutorialCondition Trigger;

	// --- Completion condition (when to advance to next step) ---
	// Default: None (manual click "Next" button)
	UPROPERTY(EditAnywhere, Category = "Tutorial|Completion")
	FTutorialCondition Completion;
};

// Tutorial data for a single level
USTRUCT(BlueprintType)
struct FLevelTutorialData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tutorial")
	int32 PresetLevelIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Tutorial")
	TArray<FTutorialStep> Steps;
};
