#include "Gameplay/Mechanisms/Door.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/StaticMesh.h"

ADoor::ADoor()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded() && MeshComp)
	{
		MeshComp->SetStaticMesh(CubeMesh.Object);
		MeshComp->SetWorldScale3D(FVector(0.9f, 0.9f, 1.0f));
	}

	DoorTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DoorTimeline"));
}

void ADoor::BeginPlay()
{
	Super::BeginPlay();

	DoorClosedZ = GetActorLocation().Z;
	DoorOpenZ = DoorClosedZ - 100.0f;

	if (DoorCurve && DoorTimeline)
	{
		FOnTimelineFloat UpdateDelegate;
		UpdateDelegate.BindUFunction(this, FName("OnDoorTimelineUpdate"));

		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(this, FName("OnDoorTimelineFinished"));

		DoorTimeline->AddInterpFloat(DoorCurve, UpdateDelegate, FName("DoorAlpha"));
		DoorTimeline->SetTimelineFinishedFunc(FinishedDelegate);
		DoorTimeline->SetPlayRate(2.0f); // 0.5 second animation
	}
}

void ADoor::SetDoorOpen(bool bOpen)
{
	if (bOpen == bIsOpen)
	{
		return;
	}

	bIsOpen = bOpen;

	if (!DoorCurve || !DoorTimeline)
	{
		SetDoorStateImmediate(bOpen);
		return;
	}

	if (bOpen)
	{
		DoorTimeline->Play();
	}
	else
	{
		DoorTimeline->Reverse();
	}
}

void ADoor::SetDoorStateImmediate(bool bOpen)
{
	bIsOpen = bOpen;

	if (DoorTimeline)
	{
		DoorTimeline->Stop();
	}

	FVector Location = GetActorLocation();
	Location.Z = bOpen ? DoorOpenZ : DoorClosedZ;
	SetActorLocation(Location);
}

void ADoor::OnActivate()
{
	Super::OnActivate();
	SetDoorOpen(true);
}

void ADoor::OnDeactivate()
{
	Super::OnDeactivate();
	SetDoorOpen(false);
}

void ADoor::OnDoorTimelineUpdate(float Alpha)
{
	FVector Location = GetActorLocation();
	Location.Z = FMath::Lerp(DoorClosedZ, DoorOpenZ, Alpha);
	SetActorLocation(Location);
}

void ADoor::OnDoorTimelineFinished()
{
	FVector Location = GetActorLocation();
	Location.Z = bIsOpen ? DoorOpenZ : DoorClosedZ;
	SetActorLocation(Location);
}
