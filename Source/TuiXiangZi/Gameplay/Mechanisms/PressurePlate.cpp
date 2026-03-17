#include "Gameplay/Mechanisms/PressurePlate.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

APressurePlate::APressurePlate()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded() && MeshComp)
	{
		MeshComp->SetStaticMesh(CylinderMesh.Object);
		MeshComp->SetWorldScale3D(FVector(0.8f, 0.8f, 0.05f));
	}
}

void APressurePlate::OnActivate()
{
	Super::OnActivate();
}

void APressurePlate::OnDeactivate()
{
	Super::OnDeactivate();
}
