// Authoritative simulation events (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEvent.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_SimEventSubsystem.generated.h"

/** Identifies one listener, to stop it later. Not valid until Listen returned it */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_SimEventListenerHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 Id = INDEX_NONE;

	bool IsValid() const { return Id != INDEX_NONE; }
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FGSTN_SimEventDynamicDelegate, const FGSTN_SimEvent&, Event);

/**
 * The listeners of a stream of events and the rule that picks which events reach them. Plain C++ so it is tested without a
 * world; the subsystem is a thin owner of one.
 *
 * A listener's query is matched against the event's Tags plus its EventType, with the usual hierarchical tag matching, so a
 * query for `Command.Attack` also matches `Command.Attack.Neutral`. An empty query matches every event.
 */
class GSTNINPUTCORE_API FGSTN_SimEventDispatcher
{
public:
	using FCallback = TFunction<void(const FGSTN_SimEvent&)>;

	FGSTN_SimEventListenerHandle AddListener(const FGameplayTagQuery& Query, FCallback Callback);

	/** @return whether the handle named a listener */
	bool RemoveListener(FGSTN_SimEventListenerHandle Handle);

	void Clear() { Listeners.Reset(); }

	int32 GetNumListeners() const { return Listeners.Num(); }

	/**
	 * Calls every listener whose query matches. Safe against a listener adding or removing listeners from its callback: the
	 * set is the one at the start of the call, except that a listener removed by an earlier callback is skipped.
	 */
	void Broadcast(const FGSTN_SimEvent& Event) const;

	/** The matching rule alone */
	static bool Matches(const FGameplayTagQuery& Query, const FGSTN_SimEvent& Event);

private:
	struct FListener
	{
		int32 Id = INDEX_NONE;
		FGameplayTagQuery Query;
		FCallback Callback;
	};

	TArray<FListener> Listeners;
	int32 NextId = 0;
};

/**
 * The world's stream of authoritative events (combat.md section 5.6, input.md section 6): the one place quests, statistics
 * and kill feeds listen, instead of simulation callbacks that run again on re-simulation. Producers publish from outside the
 * simulation, once per event and on the authority only (see FGSTN_SimEventOutbox); the input simulation publishes
 * Input.Event.CommandExecuted, other plugins publish theirs through the same subsystem.
 *
 * Nothing here is replicated: on a client the stream is empty. Feedback a client needs (a flash, a tutorial prompt) comes
 * from replicated results or predicted presentation. A listen-server host's own pawn is the authority, so the host sees its
 * own events like any other pawn's.
 */
UCLASS()
class GSTNINPUTCORE_API UGSTN_SimEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the world of WorldContextObject, null if it has none */
	static UGSTN_SimEventSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** Publishes an event to the matching listeners. Call it on the authority only, outside simulation code */
	UFUNCTION(BlueprintCallable, Category = "Input|Events")
	void Broadcast(const FGSTN_SimEvent& Event);

	/** Calls Delegate for each event the query matches (an empty query matches all) */
	UFUNCTION(BlueprintCallable, Category = "Input|Events")
	FGSTN_SimEventListenerHandle Listen(FGameplayTagQuery Query, FGSTN_SimEventDynamicDelegate Delegate);

	UFUNCTION(BlueprintCallable, Category = "Input|Events")
	bool StopListening(FGSTN_SimEventListenerHandle Handle);

	/** C++ listener: no delegate object, the callback may capture. Keep the handle to stop it before what it captures dies */
	FGSTN_SimEventListenerHandle ListenNative(const FGameplayTagQuery& Query, FGSTN_SimEventDispatcher::FCallback Callback);

private:
	FGSTN_SimEventDispatcher Dispatcher;
};
