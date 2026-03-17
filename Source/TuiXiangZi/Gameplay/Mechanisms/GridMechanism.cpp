#include "Gameplay/Mechanisms/GridMechanism.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AGridMechanism::AGridMechanism()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	SetRootComponent(MeshComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGridMechanism::CreateDynamicMaterial()
{
	if (MeshComp && MeshComp->GetNumMaterials() > 0)
	{
		DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void AGridMechanism::ApplyColor(FLinearColor Color)
{
	if (DynMaterial)
	{
		DynMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
	}
}

void AGridMechanism::OnActivate()
{
	bIsActivated = true;
	ApplyColor(CachedActiveColor);
}

void AGridMechanism::OnDeactivate()
{
	bIsActivated = false;
	ApplyColor(CachedBaseColor);
}

bool AGridMechanism::IsActivated() const
{
	return bIsActivated;
}

void AGridMechanism::SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor)
{
	CachedBaseColor = BaseColor;
	CachedActiveColor = ActiveColor;

	if (!DynMaterial)
	{
		CreateDynamicMaterial();
	}

	ApplyColor(bIsActivated ? CachedActiveColor : CachedBaseColor);
}
