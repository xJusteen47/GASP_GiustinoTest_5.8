// Authoritative simulation events (no GAS, no Network Prediction).

#include "Events/GSTN_SimEventSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

void FGSTN_SimEventOutbox::Add(FGSTN_SimEvent&& Event)
{
	if (Events.Num() >= MaxEvents)
	{
		Events.RemoveAt(0);
	}
	Events.Add(MoveTemp(Event));
}

int32 FGSTN_SimEventOutbox::Flush(bool bPublish, TFunctionRef<void(FGSTN_SimEvent&)> Publish)
{
	// Moved out first: a publisher that ends up ticking a simulation again must not append to the array being walked
	TArray<FGSTN_SimEvent> Pending = MoveTemp(Events);
	Events.Reset();

	if (!bPublish)
	{
		return 0;
	}
	for (FGSTN_SimEvent& Event : Pending)
	{
		Publish(Event);
	}
	return Pending.Num();
}

FGSTN_SimEventListenerHandle FGSTN_SimEventDispatcher::AddListener(const FGameplayTagQuery& Query, FCallback Callback)
{
	FListener& Listener = Listeners.AddDefaulted_GetRef();
	Listener.Id = NextId++;
	Listener.Query = Query;
	Listener.Callback = MoveTemp(Callback);

	FGSTN_SimEventListenerHandle Handle;
	Handle.Id = Listener.Id;
	return Handle;
}

bool FGSTN_SimEventDispatcher::RemoveListener(FGSTN_SimEventListenerHandle Handle)
{
	return Handle.IsValid() && Listeners.RemoveAll([&Handle](const FListener& Listener) { return Listener.Id == Handle.Id; }) > 0;
}

bool FGSTN_SimEventDispatcher::Matches(const FGameplayTagQuery& Query, const FGSTN_SimEvent& Event)
{
	if (Query.IsEmpty())
	{
		return true;
	}

	FGameplayTagContainer Container = Event.Tags;
	if (Event.EventType.IsValid())
	{
		Container.AddTag(Event.EventType);
	}
	return Query.Matches(Container);
}

void FGSTN_SimEventDispatcher::Broadcast(const FGSTN_SimEvent& Event) const
{
	// A callback may add or remove listeners, which reallocates Listeners: walk a copy of the ids and look each one up again
	TArray<int32, TInlineAllocator<8>> Ids;
	for (const FListener& Listener : Listeners)
	{
		Ids.Add(Listener.Id);
	}

	for (const int32 Id : Ids)
	{
		const FListener* Listener = Listeners.FindByPredicate([Id](const FListener& Candidate) { return Candidate.Id == Id; });
		if (!Listener || !Matches(Listener->Query, Event))
		{
			continue;
		}

		// Copied: the callback may remove its own listener
		const FCallback Callback = Listener->Callback;
		if (Callback)
		{
			Callback(Event);
		}
	}
}

UGSTN_SimEventSubsystem* UGSTN_SimEventSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGSTN_SimEventSubsystem>() : nullptr;
}

void UGSTN_SimEventSubsystem::Deinitialize()
{
	Dispatcher.Clear();
	Super::Deinitialize();
}

void UGSTN_SimEventSubsystem::Broadcast(const FGSTN_SimEvent& Event)
{
	Dispatcher.Broadcast(Event);
}

FGSTN_SimEventListenerHandle UGSTN_SimEventSubsystem::Listen(FGameplayTagQuery Query, FGSTN_SimEventDynamicDelegate Delegate)
{
	return Dispatcher.AddListener(Query, [Delegate](const FGSTN_SimEvent& Event) { Delegate.ExecuteIfBound(Event); });
}

bool UGSTN_SimEventSubsystem::StopListening(FGSTN_SimEventListenerHandle Handle)
{
	return Dispatcher.RemoveListener(Handle);
}

FGSTN_SimEventListenerHandle UGSTN_SimEventSubsystem::ListenNative(const FGameplayTagQuery& Query, FGSTN_SimEventDispatcher::FCallback Callback)
{
	return Dispatcher.AddListener(Query, MoveTemp(Callback));
}
