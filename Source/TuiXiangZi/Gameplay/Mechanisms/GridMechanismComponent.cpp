#include "Gameplay/Mechanisms/GridMechanismComponent.h"

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
	Super::SetGroupColor(BaseColor, ActiveColor);
	ApplyColor(bIsActivated ? CachedActiveColor : CachedBaseColor);
}
