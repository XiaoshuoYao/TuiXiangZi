#include "Gameplay/PushableBoxComponent.h"
#include "Grid/GridManager.h"
#include "Grid/TileVisualActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "Tutorial/TutorialSubsystem.h"

UStaticMeshComponent* UPushableBoxComponent::FindOwnerMeshComp() const
{
	ATileVisualActor* Owner = Cast<ATileVisualActor>(GetOwner());
	return Owner ? Owner->MeshComp : nullptr;
}

void UPushableBoxComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	GridManagerRef = Cast<AGridManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

	// Create dynamic material from owner's mesh
	UStaticMeshComponent* Mesh = FindOwnerMeshComp();
	if (Mesh)
	{
		UMaterialInterface* BaseMat = Mesh->GetMaterial(0);
		if (BaseMat)
		{
			DynamicMaterialInst = UMaterialInstanceDynamic::Create(BaseMat, Owner);
			Mesh->SetMaterial(0, DynamicMaterialInst);
		}
	}

	// Create timeline for smooth movement
	MoveTimeline = NewObject<UTimelineComponent>(Owner, TEXT("BoxMoveTimeline"));
	if (MoveTimeline)
	{
		MoveTimeline->RegisterComponent();
		Owner->AddOwnedComponent(MoveTimeline);

		if (MoveCurve)
		{
			FOnTimelineFloat UpdateDelegate;
			UpdateDelegate.BindUFunction(this, FName("OnMoveTimelineUpdate"));
			MoveTimeline->AddInterpFloat(MoveCurve, UpdateDelegate);

			FOnTimelineEvent FinishedDelegate;
			FinishedDelegate.BindUFunction(this, FName("OnMoveTimelineFinished"));
			MoveTimeline->SetTimelineFinishedFunc(FinishedDelegate);

			MoveTimeline->SetPlayRate(1.0f / MoveDuration);
		}
	}

	// Bind movement delegate
	if (GridManagerRef)
	{
		GridManagerRef->OnActorLogicalMoved.AddUObject(this, &UPushableBoxComponent::OnActorLogicalMoved);
	}
}

void UPushableBoxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GridManagerRef)
	{
		GridManagerRef->OnActorLogicalMoved.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UPushableBoxComponent::SnapToGridPos(FIntPoint GridPos)
{
	CurrentGridPos = GridPos;
	AActor* Owner = GetOwner();
	if (Owner && GridManagerRef)
	{
		FVector WorldPos = GridManagerRef->GridToWorld(GridPos);
		WorldPos.Z = Owner->GetActorLocation().Z;
		Owner->SetActorLocation(WorldPos);
	}
}

void UPushableBoxComponent::SmoothMoveTo(FVector TargetWorldPos)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (!MoveCurve || !MoveTimeline)
	{
		Owner->SetActorLocation(TargetWorldPos);
		return;
	}

	bIsMoving = true;
	MoveStartLocation = Owner->GetActorLocation();
	MoveTargetLocation = TargetWorldPos;
	MoveTargetLocation.Z = MoveStartLocation.Z;
	MoveTimeline->PlayFromStart();
}

void UPushableBoxComponent::OnMoveTimelineUpdate(float Alpha)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	FVector NewLocation = FMath::Lerp(MoveStartLocation, MoveTargetLocation, Alpha);
	Owner->SetActorLocation(NewLocation);
}

void UPushableBoxComponent::OnMoveTimelineFinished()
{
	bIsMoving = false;
	AActor* Owner = GetOwner();
	if (Owner && GridManagerRef)
	{
		FVector SnapPos = GridManagerRef->GridToWorld(CurrentGridPos);
		SnapPos.Z = Owner->GetActorLocation().Z;
		Owner->SetActorLocation(SnapPos);
	}
}

void UPushableBoxComponent::OnActorLogicalMoved(AActor* Actor, FIntPoint From, FIntPoint To)
{
	if (Actor != GetOwner()) return;
	CurrentGridPos = To;
	if (GridManagerRef)
	{
		SmoothMoveTo(GridManagerRef->GridToWorld(To));
	}

	// Notify tutorial subsystem that a box was pushed
	if (UWorld* World = GetWorld())
	{
		if (UTutorialSubsystem* TutSub = World->GetSubsystem<UTutorialSubsystem>())
		{
			TutSub->NotifyCondition(ETutorialConditionType::OnPushBox);
		}
	}
}

void UPushableBoxComponent::PlayFallIntoHoleAnim()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	Owner->SetActorHiddenInGame(true);
	Owner->SetActorEnableCollision(false);
	Owner->SetLifeSpan(1.0f);
}

void UPushableBoxComponent::SetOnPlateVisual(bool bOnPlate, FLinearColor GroupColor)
{
	if (!DynamicMaterialInst) return;
	if (bOnPlate)
	{
		DynamicMaterialInst->SetVectorParameterValue(FName("EmissiveColor"), GroupColor);
		DynamicMaterialInst->SetScalarParameterValue(FName("EmissiveIntensity"), 3.0f);
	}
	else
	{
		DynamicMaterialInst->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Black);
		DynamicMaterialInst->SetScalarParameterValue(FName("EmissiveIntensity"), 0.0f);
	}
}
