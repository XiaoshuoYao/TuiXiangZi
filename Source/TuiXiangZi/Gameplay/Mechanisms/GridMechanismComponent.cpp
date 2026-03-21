#include "Gameplay/Mechanisms/GridMechanismComponent.h"
#include "Gameplay/GroupColorIndicatorComponent.h"

void UGridMechanismComponent::OnActivate()
{
	bIsActivated = true;
	ApplyColor(CachedActiveColor);

	if (UGroupColorIndicatorComponent* Indicator = GetOwner()->FindComponentByClass<UGroupColorIndicatorComponent>())
		Indicator->SetActivated(true);
}

void UGridMechanismComponent::OnDeactivate()
{
	bIsActivated = false;
	ApplyColor(CachedBaseColor);

	if (UGroupColorIndicatorComponent* Indicator = GetOwner()->FindComponentByClass<UGroupColorIndicatorComponent>())
		Indicator->SetActivated(false);
}

void UGridMechanismComponent::SetGroupColor(FLinearColor BaseColor, FLinearColor ActiveColor)
{
	Super::SetGroupColor(BaseColor, ActiveColor);
	ApplyColor(bIsActivated ? CachedActiveColor : CachedBaseColor);
}
