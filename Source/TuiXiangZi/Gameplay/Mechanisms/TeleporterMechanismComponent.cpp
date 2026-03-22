#include "Gameplay/Mechanisms/TeleporterMechanismComponent.h"

bool UTeleporterMechanismComponent::CanSend() const
{
	return Role == 0 || Role == 1; // Bidirectional or Entry
}

bool UTeleporterMechanismComponent::CanReceive() const
{
	return Role == 0 || Role == 2; // Bidirectional or Exit
}

void UTeleporterMechanismComponent::SetRoleFromExtraParam(int32 ExtraParam)
{
	Role = FMath::Clamp(ExtraParam, 0, 2);
}
