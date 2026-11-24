// Mode verbs, Network Prediction half (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_ModeVerbTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_VerbSubsystem.generated.h"

class UGSTN_RequestableComponent;

/** C++ listeners get the full payload, aim point included (the stream's FGSTN_SimEvent has no field for it) */
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnModeVerb, const FGSTN_ModeVerbPayload& /*Payload*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGSTN_OnModeVerbRefused, const FGSTN_ModeVerbPayload& /*Payload*/, EGSTN_VerbRefusal /*Reason*/);

/**
 * The bridge from a predicted verb to one authoritative event (game-modes 2.4, spike G5). UGSTN_GA_ModeVerb writes a confirmation into
 * QueueConfirm from inside the simulation, unconditionally, on every machine and in every re-simulation; this subsystem is the only thing that
 * turns it into an event:
 *
 *   - after each fixed tick it flushes the queue (FGSTN_VerbConfirmQueue::Flush): a client throws its predictions away, a re-simulation publishes
 *     nothing, the authority publishes each confirmation once and never again;
 *   - the host validates against its own state (the requestable prop's stamps, verb, uses, reach with the lag slack, and the owner's own rule);
 *   - a valid verb is published as Message.Mode.Verb on UGSTN_SimEventSubsystem, where GSTNMission (an `Activate` objective) and GSTNRespawn
 *     (call reinforcement) listen by tag: this plugin links neither (decision D-2: the stream is the bus, no second republisher);
 *   - a refused verb publishes Message.Mode.VerbRefused and changes nothing: the client's ability already ended at its confirmation frame, which is
 *     the "one cancel" of the spec (no state to roll back).
 *
 * It also accepts the raw seam D-1 key (Message.Mode.VerbConfirmed from a timeline Event key) for target-less verbs.
 * Server state and presentation only: it writes no simulation state (CLAUDE.md rule 2). Game thread.
 */
UCLASS()
class GSTNMODEVERBSNP_API UGSTN_VerbSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_VerbSubsystem* Get(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Called by the verb ability from the simulation: appends, decides nothing. Safe to call every tick of every machine */
	void QueueConfirm(const FGSTN_VerbConfirmRequest& Request) { Queue.Add(Request); }

	/** The frame of the last fixed tick this machine ran (re-simulated ones included), INDEX_NONE before the first. Stamps are made one frame ahead of it */
	int32 GetSimFrame() const { return LastFrame; }

	/** Host: a verb the host validated and published (every verb, from either seam) */
	FGSTN_OnModeVerb OnModeVerb;
	/** Host: a verb the host refused */
	FGSTN_OnModeVerbRefused OnModeVerbRefused;

	/**
	 * Host only: validates and publishes one confirmation now, bypassing the queue. What the flush calls; public for tests and scripted flows.
	 * @return None when the verb was accepted and published, else why it was refused (and the refusal was published)
	 */
	EGSTN_VerbRefusal ProcessConfirmed(const FGSTN_VerbConfirmRequest& Request);

private:
	void HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating);
	/** Seam D-1: a timeline key named Message.Mode.VerbConfirmed was crossed (published once by the combat outbox) */
	void HandleRawConfirmed(const FGSTN_SimEvent& Event);
	bool HasAuthority() const;

	/** The requestable the verb acts on: the focus key when the request names one, else the nearest live prop that accepts the verb in reach */
	UGSTN_RequestableComponent* ResolveRequestable(const FGSTN_VerbConfirmRequest& Request, const AActor* Instigator, EGSTN_VerbRefusal& OutRefusal) const;

	void PublishRefusal(const FGSTN_ModeVerbPayload& Payload, EGSTN_VerbRefusal Reason);

	FGSTN_VerbConfirmQueue Queue;
	int32 LastFrame = INDEX_NONE;

	FDelegateHandle PostFixedTickHandle;
	FGSTN_SimEventListenerHandle RawListener;
};
