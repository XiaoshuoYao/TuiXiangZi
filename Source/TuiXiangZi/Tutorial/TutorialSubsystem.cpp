#include "Tutorial/TutorialSubsystem.h"
#include "Tutorial/TutorialDataAsset.h"
#include "UI/TutorialWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

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
		// Action-based types (OnPlayerMove, OnPushBox, etc.) match by type alone
		return true;
	}
}

// =========================================================================

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

// ---- Unified action notification ----
void UTutorialSubsystem::NotifyCondition(ETutorialConditionType Type)
{
	if (!IsTutorialActive() || bPaused) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];

	if (bWaitingForTrigger)
	{
		TryMatchTrigger(Step.Trigger, Type, CachedStepCount, CachedPlayerPos, NAME_None);
	}
	else
	{
		TryMatchCompletion(Step.Completion, Type, CachedStepCount, CachedPlayerPos, NAME_None);
	}
}

// ---- Parameterized notifications ----
void UTutorialSubsystem::NotifyGameplayEvent(FName EventTag)
{
	if (!IsTutorialActive() || bPaused) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];

	if (bWaitingForTrigger)
	{
		TryMatchTrigger(Step.Trigger, ETutorialConditionType::OnGameplayEvent, CachedStepCount, CachedPlayerPos, EventTag);
	}
	else
	{
		TryMatchCompletion(Step.Completion, ETutorialConditionType::OnGameplayEvent, CachedStepCount, CachedPlayerPos, EventTag);
	}
}

void UTutorialSubsystem::NotifyStepCountChanged(int32 NewCount)
{
	CachedStepCount = NewCount;

	if (!IsTutorialActive() || bPaused) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];

	if (bWaitingForTrigger)
	{
		TryMatchTrigger(Step.Trigger, ETutorialConditionType::AfterSteps, CachedStepCount, CachedPlayerPos, NAME_None);
	}
	else
	{
		TryMatchCompletion(Step.Completion, ETutorialConditionType::AfterSteps, CachedStepCount, CachedPlayerPos, NAME_None);
	}
}

void UTutorialSubsystem::NotifyPlayerMoved(FIntPoint NewGridPos)
{
	CachedPlayerPos = NewGridPos;

	if (!IsTutorialActive() || bPaused) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];

	if (bWaitingForTrigger)
	{
		TryMatchTrigger(Step.Trigger, ETutorialConditionType::OnGridPosition, CachedStepCount, CachedPlayerPos, NAME_None);
	}
	else
	{
		TryMatchCompletion(Step.Completion, ETutorialConditionType::OnGridPosition, CachedStepCount, CachedPlayerPos, NAME_None);
	}
}

void UTutorialSubsystem::AdvanceTutorial()
{
	if (!IsTutorialActive() || bPaused) return;

	const FTutorialStep& Step = ActiveTutorial->Steps[CurrentStepIndex];
	if (Step.Completion.Type == ETutorialConditionType::None)
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
	bool bManualAdvance = (Step.Completion.Type == ETutorialConditionType::None);

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
	else if (DoesConditionMatch(NextStep.Trigger, NextStep.Trigger.Type, CachedStepCount, CachedPlayerPos, NAME_None))
	{
		// Parameterized conditions that may already be satisfied (e.g., already at position)
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
