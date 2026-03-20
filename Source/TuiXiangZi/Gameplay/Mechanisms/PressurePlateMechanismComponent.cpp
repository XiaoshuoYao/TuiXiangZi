#include "Gameplay/Mechanisms/PressurePlateMechanismComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void UPressurePlateMechanismComponent::OnActivate()
{
	Super::OnActivate();
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(FName("EmissiveIntensity"), 5.0f);
		DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), CachedActiveColor);
	}
}

void UPressurePlateMechanismComponent::OnDeactivate()
{
	Super::OnDeactivate();
	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(FName("EmissiveIntensity"), 0.5f);
		DynMaterial->SetVectorParameterValue(FName("EmissiveColor"), CachedBaseColor);
	}
}
