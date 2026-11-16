// Authoritative simulation events (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GSTN_SimEvent.generated.h"

/**
 * Something that happened in a simulation and must be counted once: a command executed, later a hit landed or a timeline
 * finished (combat.md section 5.6). Quests, statistics, kill feeds and analytics listen to these, never to simulation
 * callbacks, which run again on a client that re-simulates.
 *
 * Tags holds everything a listener may filter on (the command tag, the owner's context tags, the ability's tags, weapon
 * and kit tags...), so one FGameplayTagQuery can say "10 x CommandExecuted with Command.Attack.CrouchDirectional while
 * Input.Context.Airborne". EventType is matched together with Tags.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_SimEvent
{
	GENERATED_BODY()

	/** What happened (Input.Event.CommandExecuted...). Declared by whoever produces the event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FGameplayTag EventType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FGameplayTagContainer Tags;

	/** Network Prediction frame the event happened on, INDEX_NONE if it has none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	int32 Frame = INDEX_NONE;

	/** The pawn that did it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	TWeakObjectPtr<AActor> Instigator;

	/** The pawn it happened to, if any */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	TWeakObjectPtr<AActor> Target;

	/** Damage, a count... whatever the event type defines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	float Magnitude = 0.f;
};

/**
 * Events written by a simulation tick, waiting to be published. A plain array outside the sync state: the tick appends to
 * it unconditionally (no role or re-simulation branch, so the simulation stays deterministic), and the host publishes it
 * after the Network Prediction tick, on the authority only (Flush). Elsewhere the same events come from predicted frames
 * and re-simulations, so they are thrown away unseen.
 *
 * Capped: a pawn whose host never flushes (an interpolated proxy does not tick, but a misconfigured one might) must not
 * grow without bound. The oldest events are dropped first.
 */
struct GSTNINPUTCORE_API FGSTN_SimEventOutbox
{
	static constexpr int32 MaxEvents = 64;

	TArray<FGSTN_SimEvent> Events;

	void Add(FGSTN_SimEvent&& Event);

	bool IsEmpty() const { return Events.IsEmpty(); }

	/**
	 * Empties the outbox, handing each event to Publish when bPublish is set. bPublish is "this machine is the authority"
	 * and nothing else: a listen-server host's own pawn is the authority and has a local player, and its events must be
	 * published like any other pawn's. Never derive it from "no local player".
	 * @return the number of events published
	 */
	int32 Flush(bool bPublish, TFunctionRef<void(FGSTN_SimEvent&)> Publish);
};
