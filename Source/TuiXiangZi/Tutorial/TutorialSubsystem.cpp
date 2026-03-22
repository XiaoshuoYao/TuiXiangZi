#include "Tutorial/TutorialSubsystem.h"
#include "Tutorial/TutorialDataAsset.h"
#include "UI/TutorialWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Events/GameEventBus.h"
#include "Events/GameEventTags.h"

// ---- Helper: does a condition match given the incoming notification? ----
static bool DoesConditionMatch(const FTutorialCondition& Cond, ETutorialConditionType InType,
	int32 StepCount, FIntPoint PlayerPos, FName EventTag)
{
	if (Cond.Type != InType) return false;

	switch (Cond.Type)
	{
	case ETutorialConditionType::AfterSteps:
		return StepCount >= Cond.StepCount;
	case ETutorialConditionType::OnGridPosition:
		return PlayerPos == Cond.GridPos;
	case ETutorialConditionType::OnGameplayEvent:
		return EventTag == Cond.EventTag;
	default:
		return true;
	}
}

// ---- Map EventBus tag to ETutorialConditionType ----
// Only special-logic conditions get dedicated types; everything else is OnGameplayEvent.
static ETutorialConditionType EventTagToConditionType(FName Tag)
{
	if (Tag == GameEventTags::StepCountChanged) return ETutorialConditionType::AfterSteps;
	return ETutorialConditionType::OnGameplayEvent;
}

// =========================================================================

void UTutorialSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UGameEventBus* EventBus = InWorld.GetSubsystem<UGameEventBus>();
	if (!EventBus) return;

	// Subscribe to all known event tags so any can be used in tutorial conditions
	auto Sub = [&](FName Tag)
	{
		EventBus->Subscribe(Tag, FOnGameEvent::FDelegate::CreateUObject(this, &UTutorialSubsystem::OnGameEvent));
	};

	Sub(GameEventTags::ActorMoved);
	Sub(GameEventTags::PlayerEnteredGoal);
	Sub(GameEventTags::PitFilled);
	Sub(GameEventTags::StepCountChanged);
	Sub(GameEventTags::LevelCompleted);
	Sub(GameEventTags::PlayerMoved);
	Sub(GameEventTags::PushedBox);
	Sub(GameEventTags::Undone);
	Sub(GameEventTags::Reset);
	Sub(GameEventTags::DoorOpened);
	Sub(GameEventTags::Teleported);
	Sub(GameEventTags::EditorBrushChanged);
	Sub(GameEventTags::EditorCellPainted);
	Sub(GameEventTags::EditorCellErased);
	Sub(GameEventTags::EditorGroupCreated);
	Sub(GameEventTags::EditorModeChanged);
	Sub(GameEventTags::EditorNewLevel);
	Sub(GameEventTags::EditorLevelSaved);
	Sub(GameEventTags::EditorLevelLoaded);
	Sub(GameEventTags::EditorLevelTested);
	Sub(GameEventTags::EditorGridBoundsChanged);
	Sub(GameEventTags::EditorTeleporterDirectionChanged);
}

void UTutorialSubsystem::OnGameEvent(FName EventTag, const FGameEventPayload& Payload)
{
	// Update cached state from payload
	if (EventTag == GameEventTags::StepCountChanged)
	{
		CachedStepCount = Payload.IntParam;
	}
	else if (EventTag == GameEventTags::PlayerMoved)
	{
		CachedPlayerPos = Payload.GridPos;
	}

	if (!IsTutorialActive() || bPaused) return;

	// Record event for retroactive completion checks
	FiredEventTags.Add(EventTag);

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];
	ETutorialConditionType CondType = EventTagToConditionType(EventTag);

	// For Player.Moved, also try OnGridPosition match
	if (EventTag == GameEventTags::PlayerMoved)
	{
		if (bWaitingForTrigger)
		{
			TryMatchTrigger(Step.Trigger, ETutorialConditionType::OnGridPosition, CachedStepCount, CachedPlayerPos, NAME_None);
		}
		else
		{
			TryMatchCompletion(Step.Completion, ETutorialConditionType::OnGridPosition, CachedStepCount, CachedPlayerPos, NAME_None);
		}
		// Fall through to also try OnGameplayEvent match for "Player.Moved"
	}

	// All events (including Player.Moved) try OnGameplayEvent and AfterSteps match
	if (bWaitingForTrigger)
	{
		TryMatchTrigger(Step.Trigger, CondType, CachedStepCount, CachedPlayerPos, EventTag);
	}
	else
	{
		TryMatchCompletion(Step.Completion, CondType, CachedStepCount, CachedPlayerPos, EventTag);
	}

	// Scan all future steps for trigger pre-satisfaction (regardless of current state)
	if (IsTutorialActive())
	{
		RecordFutureTriggers(CondType, EventTag);
	}
}

void UTutorialSubsystem::SetTutorialConfig(UTutorialDataAsset* Data, TSubclassOf<UTutorialWidget> WidgetClass)
{
	TutorialData = Data;
	DefaultWidgetClass = WidgetClass;
}

void UTutorialSubsystem::StartTutorial(int32 PresetLevelIndex)
{
	DismissTutorial();

	if (!TutorialData) return;

	ActiveTutorial = TutorialData->FindTutorialForLevel(PresetLevelIndex);
	if (!ActiveTutorial || ActiveTutorial->Steps.Num() == 0) return;

	bIsEditorTutorial = false;
	CurrentStepIndex = 0;
	CachedStepCount = 0;
	bPaused = false;
	bPendingShow = false;

	const FTutorialStep& FirstStep = ActiveTutorial->Steps[0];
	if (FirstStep.Trigger.Type == ETutorialConditionType::Immediate ||
		FirstStep.Trigger.Type == ETutorialConditionType::None)
	{
		bWaitingForTrigger = false;
		ShowCurrentStep();
	}
	else
	{
		bWaitingForTrigger = true;
	}
}

void UTutorialSubsystem::StartEditorTutorial()
{
	DismissTutorial();

	if (!TutorialData) return;
	if (TutorialData->EditorTutorialSteps.Num() == 0) return;

	// Build a temporary FLevelTutorialData to reuse the same flow
	EditorTutorialStorage.Steps = TutorialData->EditorTutorialSteps;
	ActiveTutorial = &EditorTutorialStorage;

	bIsEditorTutorial = true;
	CurrentStepIndex = 0;
	CachedStepCount = 0;
	bPaused = false;
	bPendingShow = false;

	const FTutorialStep& FirstStep = ActiveTutorial->Steps[0];
	if (FirstStep.Trigger.Type == ETutorialConditionType::Immediate ||
		FirstStep.Trigger.Type == ETutorialConditionType::None)
	{
		bWaitingForTrigger = false;
		ShowCurrentStep();
	}
	else
	{
		bWaitingForTrigger = true;
	}
}

void UTutorialSubsystem::AdvanceTutorial()
{
	if (!IsTutorialActive() || bPaused) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];
	// Allow manual advance if completion is None OR if it was already satisfied (overridden to manual)
	if (Step.Completion.Type == ETutorialConditionType::None || IsConditionAlreadySatisfied(Step.Completion))
	{
		TryAdvanceToNextStep();
	}
}

void UTutorialSubsystem::SetPaused(bool bInPaused)
{
	if (bPaused == bInPaused) return;
	bPaused = bInPaused;

	if (!IsTutorialActive()) return;

	if (bPaused)
	{
		if (TutorialWidget && TutorialWidget->IsInViewport())
		{
			bPendingShow = true;
			TutorialWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		if (bPendingShow && TutorialWidget)
		{
			TutorialWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			bPendingShow = false;
		}
	}
}

void UTutorialSubsystem::DismissTutorial()
{
	HideWidget();
	ActiveTutorial = nullptr;
	CurrentStepIndex = -1;
	bWaitingForTrigger = false;
	bPendingShow = false;
	bIsEditorTutorial = false;
	PreSatisfiedTriggers.Empty();
	FiredEventTags.Empty();
}

bool UTutorialSubsystem::IsTutorialActive() const
{
	return ActiveTutorial != nullptr && CurrentStepIndex >= 0;
}

void UTutorialSubsystem::ShowCurrentStep()
{
	if (!IsTutorialActive()) return;

	if (bPaused)
	{
		bPendingShow = true;
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];

	// Determine widget class
	TSubclassOf<UTutorialWidget> WidgetClass = Step.WidgetClassOverride ? Step.WidgetClassOverride : DefaultWidgetClass;
	if (!WidgetClass) return;

	// Create widget if needed (or if class changed)
	if (!TutorialWidget || TutorialWidget->GetClass() != WidgetClass)
	{
		HideWidget();
		TutorialWidget = CreateWidget<UTutorialWidget>(PC, WidgetClass);
		if (!TutorialWidget) return;

		TutorialWidget->OnAdvanced.AddDynamic(this, &UTutorialSubsystem::AdvanceTutorial);
	}

	// Configure widget
	TutorialWidget->SetTutorialText(Step.TutorialText);

	bool bIsLastStep = (CurrentStepIndex == ActiveTutorial->Steps.Num() - 1);
	// If the completion condition is already satisfied, fall back to manual advance
	// so the player still sees the step and clicks "Next"
	bool bManualAdvance = (Step.Completion.Type == ETutorialConditionType::None)
		|| IsConditionAlreadySatisfied(Step.Completion);

	if (bManualAdvance)
	{
		TutorialWidget->SetNextButtonVisible(true);
		// "知道了" for last step, "下一步" otherwise
		TutorialWidget->SetButtonText(FText::FromString(bIsLastStep ? TEXT("\x77E5\x9053\x4E86") : TEXT("\x4E0B\x4E00\x6B65")));
	}
	else
	{
		TutorialWidget->SetNextButtonVisible(false);
	}

	if (!TutorialWidget->IsInViewport())
	{
		TutorialWidget->AddToViewport(50);
	}
	TutorialWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Set input mode: if there's a button to click, enable UI input
	if (bManualAdvance)
	{
		SetUIInputMode(true);
	}
}

void UTutorialSubsystem::HideWidget()
{
	if (TutorialWidget)
	{
		SetUIInputMode(false);
		TutorialWidget->RemoveFromParent();
		TutorialWidget = nullptr;
	}
}

void UTutorialSubsystem::TryAdvanceToNextStep()
{
	if (!IsTutorialActive()) return;

	HideWidget();

	CurrentStepIndex++;
	if (CurrentStepIndex >= ActiveTutorial->Steps.Num())
	{
		DismissTutorial();
		return;
	}

	const FTutorialStep& NextStep = ActiveTutorial->Steps[CurrentStepIndex];

	// Check if trigger is immediately satisfied
	if (NextStep.Trigger.Type == ETutorialConditionType::Immediate ||
		NextStep.Trigger.Type == ETutorialConditionType::None)
	{
		bWaitingForTrigger = false;
		ShowCurrentStep();
	}
	else if (PreSatisfiedTriggers.Contains(CurrentStepIndex) ||
		IsConditionAlreadySatisfied(NextStep.Trigger))
	{
		// Trigger was pre-satisfied (event fired while earlier step was displayed),
		// or parameterized condition is already met against cached state
		PreSatisfiedTriggers.Remove(CurrentStepIndex);
		bWaitingForTrigger = false;
		ShowCurrentStep();
	}
	else
	{
		bWaitingForTrigger = true;
	}
}

void UTutorialSubsystem::TryMatchTrigger(const FTutorialCondition& Condition, ETutorialConditionType InType,
	int32 StepCount, FIntPoint PlayerPos, FName EventTag)
{
	if (!bWaitingForTrigger) return;

	if (DoesConditionMatch(Condition, InType, StepCount, PlayerPos, EventTag))
	{
		bWaitingForTrigger = false;
		ShowCurrentStep();
	}
}

void UTutorialSubsystem::TryMatchCompletion(const FTutorialCondition& Condition, ETutorialConditionType InType,
	int32 StepCount, FIntPoint PlayerPos, FName EventTag)
{
	if (bWaitingForTrigger) return;
	if (Condition.Type == ETutorialConditionType::None) return; // manual advance only

	if (DoesConditionMatch(Condition, InType, StepCount, PlayerPos, EventTag))
	{
		TryAdvanceToNextStep();
	}
}

bool UTutorialSubsystem::IsConditionAlreadySatisfied(const FTutorialCondition& Condition) const
{
	switch (Condition.Type)
	{
	case ETutorialConditionType::AfterSteps:
		return CachedStepCount >= Condition.StepCount;
	case ETutorialConditionType::OnGridPosition:
		return CachedPlayerPos == Condition.GridPos;
	case ETutorialConditionType::OnGameplayEvent:
		return FiredEventTags.Contains(Condition.EventTag);
	default:
		return false;
	}
}

void UTutorialSubsystem::RecordFutureTriggers(ETutorialConditionType CondType, FName EventTag)
{
	for (int32 i = CurrentStepIndex + 1; i < ActiveTutorial->Steps.Num(); i++)
	{
		if (PreSatisfiedTriggers.Contains(i)) continue;

		const FTutorialCondition& Trigger = ActiveTutorial->Steps[i].Trigger;
		if (DoesConditionMatch(Trigger, CondType, CachedStepCount, CachedPlayerPos, EventTag))
		{
			PreSatisfiedTriggers.Add(i);
		}
		// For Player.Moved, also check OnGridPosition triggers
		if (EventTag == GameEventTags::PlayerMoved && !PreSatisfiedTriggers.Contains(i))
		{
			if (DoesConditionMatch(Trigger, ETutorialConditionType::OnGridPosition, CachedStepCount, CachedPlayerPos, NAME_None))
			{
				PreSatisfiedTriggers.Add(i);
			}
		}
	}
}

void UTutorialSubsystem::SetUIInputMode(bool bUIMode)
{
	// Editor already manages its own input mode and cursor — don't interfere
	if (bIsEditorTutorial) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	if (bUIMode)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	else
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}
}
