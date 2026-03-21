#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Events/GameEventPayload.h"
#include "GameEventBus.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameEvent, FName /*EventTag*/, const FGameEventPayload& /*Payload*/);

UCLASS()
class TUIXIANGZI_API UGameEventBus : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Broadcast(FName EventTag, const FGameEventPayload& Payload = FGameEventPayload());
	FDelegateHandle Subscribe(FName EventTag, const FOnGameEvent::FDelegate& Delegate);
	void Unsubscribe(FName EventTag, FDelegateHandle Handle);
	void UnsubscribeAll(FName EventTag, const void* Object);
	void UnsubscribeAllForObject(const void* Object);

private:
	TMap<FName, FOnGameEvent> EventDelegates;
	// Reverse index: Object -> list of (EventTag, Handle) for efficient UnsubscribeAllForObject
	TMap<const void*, TArray<TPair<FName, FDelegateHandle>>> ObjectSubscriptions;
};
