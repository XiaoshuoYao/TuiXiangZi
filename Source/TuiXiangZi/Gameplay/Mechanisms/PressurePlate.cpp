#include "Gameplay/Mechanisms/PressurePlate.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	// Enhanced glow on activation
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(FName("EmissiveIntensity"), 5.0f);
		DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), CachedActiveColor);
	}
}

void APressurePlate::OnDeactivate()
{
	Super::OnDeactivate();
	// Dim glow on deactivation
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(FName("EmissiveIntensity"), 0.5f);
		DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), CachedBaseColor);
	}
}
