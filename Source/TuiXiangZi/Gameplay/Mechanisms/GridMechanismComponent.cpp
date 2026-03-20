#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "Grid/TileVisualActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UStaticMeshComponent* UGridMechanismComponent::FindOwnerMeshComp() const
{
	ATileVisualActor* Owner = Cast<ATileVisualActor>(GetOwner());
	return Owner ? Owner->MeshComp : nullptr;
}

void UGridMechanismComponent::CreateDynamicMaterial()
{
	UStaticMeshComponent* Mesh = FindOwnerMeshComp();
	if (Mesh && Mesh->GetNumMaterials() > 0)
	{
		DynMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void UGridMechanismComponent::ApplyColor(FLinearColor Color)
{
	if (DynMaterial)
	{
		DynMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
	}
}

void UGridMechanismComponent::OnActivate()
{
	bIsActivated = true;
	ApplyColor(CachedActiveColor);
}

void UGridMechanismComponent::OnDeactivate()
{
	bIsActivated = false;
	ApplyColor(CachedBaseColor);
}

void UGridMechanismComponent::SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor)
{
	CachedBaseColor = BaseColor;
	CachedActiveColor = ActiveColor;

	if (!DynMaterial)
	{
		CreateDynamicMaterial();
	}

	ApplyColor(bIsActivated ? CachedActiveColor : CachedBaseColor);
}
