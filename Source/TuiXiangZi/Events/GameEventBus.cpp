#include "Events/GameEventBus.h"

void UGameEventBus::Broadcast(FName EventTag, const FGameEventPayload& Payload)
{
	if (FOnGameEvent* Delegate = EventDelegates.Find(EventTag))
	{
		Delegate->Broadcast(EventTag, Payload);
	}
}

FDelegateHandle UGameEventBus::Subscribe(FName EventTag, const FOnGameEvent::FDelegate& Delegate)
{
	FDelegateHandle Handle = EventDelegates.FindOrAdd(EventTag).Add(Delegate);

	// Track in reverse index for UnsubscribeAllForObject
	const void* Object = Delegate.GetUObject();
	if (Object)
	{
		ObjectSubscriptions.FindOrAdd(Object).Add(TPair<FName, FDelegateHandle>{EventTag, Handle});
	}

	return Handle;
}

void UGameEventBus::Unsubscribe(FName EventTag, FDelegateHandle Handle)
{
	if (FOnGameEvent* Delegate = EventDelegates.Find(EventTag))
	{
		Delegate->Remove(Handle);
	}

	// Remove from reverse index
	for (auto& Pair : ObjectSubscriptions)
	{
		Pair.Value.RemoveAll([&](const TPair<FName, FDelegateHandle>& Entry)
		{
			return Entry.Key == EventTag && Entry.Value == Handle;
		});
	}
}

void UGameEventBus::UnsubscribeAll(FName EventTag, const void* Object)
{
	if (FOnGameEvent* Delegate = EventDelegates.Find(EventTag))
	{
		Delegate->RemoveAll(Object);
	}

	// Clean reverse index
	if (auto* Subs = ObjectSubscriptions.Find(Object))
	{
		Subs->RemoveAll([&](const TPair<FName, FDelegateHandle>& Entry)
		{
			return Entry.Key == EventTag;
		});
	}
}

void UGameEventBus::UnsubscribeAllForObject(const void* Object)
{
	if (auto* Subs = ObjectSubscriptions.Find(Object))
	{
		for (const auto& Entry : *Subs)
		{
			if (FOnGameEvent* Delegate = EventDelegates.Find(Entry.Key))
			{
				Delegate->Remove(Entry.Value);
			}
		}
		ObjectSubscriptions.Remove(Object);
	}
}
