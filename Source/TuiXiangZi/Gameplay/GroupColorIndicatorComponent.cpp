#include "Gameplay/GroupColorIndicatorComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void UGroupColorIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bColorPending)
	{
		bColorPending = false;
		OnUpdateVisual(bIsActivated ? CachedActiveColor : CachedBaseColor, bIsActivated);
	}
}

void UGroupColorIndicatorComponent::SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor)
{
	CachedBaseColor = BaseColor;
	CachedActiveColor = ActiveColor;
	OnUpdateVisual(bIsActivated ? CachedActiveColor : CachedBaseColor, bIsActivated);
}

void UGroupColorIndicatorComponent::SetActivated(bool bNewActivated)
{
	bIsActivated = bNewActivated;
	OnUpdateVisual(bIsActivated ? CachedActiveColor : CachedBaseColor, bIsActivated);
}

void UGroupColorIndicatorComponent::OnUpdateVisual_Implementation(FLinearColor CurrentColor, bool bActivated)
{
	if (!DynMaterial && GetNumMaterials() > 0)
	{
		DynMaterial = CreateAndSetMaterialInstanceDynamic(0);
	}

	if (DynMaterial)
	{
		DynMaterial->SetVectorParameterValue(FName("Color"), CurrentColor);
		DynMaterial->SetScalarParameterValue(FName("Intensity"), bActivated ? ActiveIntensity : InactiveIntensity);
	}
	else
	{
		bColorPending = true;
	}
}
