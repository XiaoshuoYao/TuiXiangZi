#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tutorial/TutorialTypes.h"
#include "Events/GameEventPayload.h"
#include "TutorialSubsystem.generated.h"

class UTutorialDataAsset;
class UTutorialWidget;
class UGameEventBus;

UCLASS()
class TUIXIANGZI_API UTutorialSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Initialize configuration (called by GameMode in BeginPlay)
	void SetTutorialConfig(UTutorialDataAsset* Data, TSubclassOf<UTutorialWidget> WidgetClass);

	// Start tutorial for a gameplay level
	void StartTutorial(int32 PresetLevelIndex);

	// Start tutorial for the level editor
	void StartEditorTutorial();

	// Manual advance (widget Next button callback)
	UFUNCTION()
	void AdvanceTutorial();

	// Pause/resume tutorial (when pause menu opens/closes)
	void SetPaused(bool bInPaused);

	// Dismiss and clean up
	void DismissTutorial();

	bool IsTutorialActive() const;

private:
	UPROPERTY()
	UTutorialDataAsset* TutorialData = nullptr;

	UPROPERTY()
	TSubclassOf<UTutorialWidget> DefaultWidgetClass;

	UPROPERTY()
	UTutorialWidget* TutorialWidget = nullptr;

	const FLevelTutorialData* ActiveTutorial = nullptr;
	int32 CurrentStepIndex = -1;
	bool bWaitingForTrigger = false;
	bool bPaused = false;
	bool bPendingShow = false;

	void ShowCurrentStep();
	void HideWidget();
	void TryAdvanceToNextStep();

	// Try to satisfy the current step's trigger or completion
	void TryMatchTrigger(const FTutorialCondition& Condition, ETutorialConditionType InType, int32 StepCount, FIntPoint PlayerPos, FName EventTag);
	void TryMatchCompletion(const FTutorialCondition& Condition, ETutorialConditionType InType, int32 StepCount, FIntPoint PlayerPos, FName EventTag);

	void SetUIInputMode(bool bUIMode);

	// EventBus handler for all subscribed events
	void OnGameEvent(FName EventTag, const FGameEventPayload& Payload);

	// When true, skip input mode changes (editor already manages its own cursor/input)
	bool bIsEditorTutorial = false;

	// Storage for editor tutorial data (so ActiveTutorial can point to it)
	FLevelTutorialData EditorTutorialStorage;

	// Cached state for condition checks
	int32 CachedStepCount = 0;
	FIntPoint CachedPlayerPos = FIntPoint::ZeroValue;
};
