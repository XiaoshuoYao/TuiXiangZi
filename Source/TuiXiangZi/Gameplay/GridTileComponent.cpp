#include "Gameplay/GridTileComponent.h"
#include "Grid/TileActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UStaticMeshComponent* UGridTileComponent::FindOwnerMeshComp() const
{
	ATileActor* Owner = Cast<ATileActor>(GetOwner());
	return Owner ? Owner->MeshComp : nullptr;
}

void UGridTileComponent::CreateDynamicMaterial()
{
	UStaticMeshComponent* Mesh = FindOwnerMeshComp();
	if (Mesh && Mesh->GetNumMaterials() > 0)
	{
		DynMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void UGridTileComponent::ApplyColor(FLinearColor Color)
{
	if (DynMaterial)
	{
		DynMaterial->SetVectorParameterValue(FName("BaseColor"), Color);
	}
}

void UGridTileComponent::SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor)
{
	CachedBaseColor = BaseColor;
	CachedActiveColor = ActiveColor;

	if (!DynMaterial)
	{
		CreateDynamicMaterial();
	}

	ApplyColor(CachedBaseColor);
}
