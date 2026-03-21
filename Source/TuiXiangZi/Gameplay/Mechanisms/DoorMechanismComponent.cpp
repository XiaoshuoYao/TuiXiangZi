#include "Gameplay/Mechanisms/DoorMechanismComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Events/GameEventBus.h"
#include "Events/GameEventTags.h"

void UDoorMechanismComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	DoorClosedZ = Owner->GetActorLocation().Z;
	DoorOpenZ = DoorClosedZ + 300.0f;

	// Timeline must be created at runtime for component-owned timelines
	DoorTimeline = NewObject<UTimelineComponent>(Owner, TEXT("DoorTimeline"));
	if (DoorTimeline)
	{
		DoorTimeline->RegisterComponent();
		Owner->AddOwnedComponent(DoorTimeline);

		if (DoorCurve)
		{
			FOnTimelineFloat UpdateDelegate;
			UpdateDelegate.BindUFunction(this, FName("OnDoorTimelineUpdate"));

			FOnTimelineEvent FinishedDelegate;
			FinishedDelegate.BindUFunction(this, FName("OnDoorTimelineFinished"));

			DoorTimeline->AddInterpFloat(DoorCurve, UpdateDelegate, FName("DoorAlpha"));
			DoorTimeline->SetTimelineFinishedFunc(FinishedDelegate);
			DoorTimeline->SetPlayRate(2.0f);
		}
	}
}

void UDoorMechanismComponent::SetDoorOpen(bool bOpen)
{
	if (bOpen == bIsOpen) return;

	bIsOpen = bOpen;

	// Broadcast DoorOpened event via EventBus
	if (bOpen)
	{
		if (UGameEventBus* EventBus = GetWorld()->GetSubsystem<UGameEventBus>())
		{
			EventBus->Broadcast(GameEventTags::DoorOpened);
		}
	}

	if (!DoorCurve || !DoorTimeline)
	{
		SetDoorStateImmediate(bOpen);
		return;
	}

	if (bOpen)
		DoorTimeline->Play();
	else
		DoorTimeline->Reverse();
}

void UDoorMechanismComponent::SetDoorStateImmediate(bool bOpen)
{
	bIsOpen = bOpen;

	if (DoorTimeline)
		DoorTimeline->Stop();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector Location = Owner->GetActorLocation();
	Location.Z = bOpen ? DoorOpenZ : DoorClosedZ;
	Owner->SetActorLocation(Location);
}

void UDoorMechanismComponent::OnActivate()
{
	Super::OnActivate();
	SetDoorOpen(true);
}

void UDoorMechanismComponent::OnDeactivate()
{
	Super::OnDeactivate();
	SetDoorOpen(false);
}

void UDoorMechanismComponent::OnDoorTimelineUpdate(float Alpha)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector Location = Owner->GetActorLocation();
	Location.Z = FMath::Lerp(DoorClosedZ, DoorOpenZ, Alpha);
	Owner->SetActorLocation(Location);
}

void UDoorMechanismComponent::OnDoorTimelineFinished()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector Location = Owner->GetActorLocation();
	Location.Z = bIsOpen ? DoorOpenZ : DoorClosedZ;
	Owner->SetActorLocation(Location);
}
