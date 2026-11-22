// Held objects, world side (needs the GAS-NPP fork for the fixed tick it reads).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_HeldTypes.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_LinkWatchdog.h"
#include "Subsystems/WorldSubsystem.h"

#include "GSTN_HeldSubsystem.generated.h"

class AGSTN_Projectile;
class UGSTN_HeldObjectComponent;

/**
 * The host's held objects (M10 slice 1). Objects register themselves; on the host, after the Network Prediction fixed tick of every
 * frame F (never on a resimulated frame, and the host never resimulates):
 * - queued pickup requests are resolved with FGSTN_HeldRules::ResolvePickups (earliest request frame, then lower holder entity id;
 *   nearest object in its radius), and the winners' objects become Held;
 * - a held object whose holder is gone drops where it is;
 * - a flying object follows its GSTNProjectile shot: when the shot's segment from F - 1 to F crosses the world (WorldStatic) the shot is
 *   removed and the object lands there; when the shot ended (hit or lifetime) the object drops to the floor below where it was.
 *
 * - a held object whose holder asked to put it down (RequestDrop), got a break tag (FGSTN_HeldBreakRules::DropWhenHolderHasAny: hitstun, tumble,
 *   grabbed) or a break event (DropOnHolderEvents: KO) is placed in front of the holder on the floor (slice 2).
 *
 * The link (M10 G1): every start and end of a hold is a GSTNLink command on the holder's Hand channel (FGSTN_HeldLinkType, stamped with the
 * holder's latest frame), so the holder's Hand link and its projected Combat.Held.Carrying are simulation state: predicted, rolled back, the
 * same on every machine for a frame. The object half stays the host's: pickups and throws are host side effects of the abilities (like a
 * projectile's fire), and a client only reads the replicated object state to draw and to build its ghost. The subsystem is the link watchdog's
 * resolver for the Object kind: a hold whose object is destroyed ends there.
 *
 * Pickup target (M10 slice 5): a request takes the object whose record GSTNLink's interact probe focused for the holder on the request's frame
 * (UGSTN_InteractSubsystem::FindProbeFocus); without a focused record, the nearest object in reach (the race of FGSTN_HeldRules::ResolvePickups).
 */
struct FGSTN_LinkState;

UCLASS()
class GSTNHELDRUNTIME_API UGSTN_HeldSubsystem : public UWorldSubsystem, public IGSTN_LinkPartnerResolver
{
	GENERATED_BODY()

public:
	static UGSTN_HeldSubsystem* Get(const UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** IGSTN_LinkPartnerResolver (kind Object): valid while the object exists, at its actor's location */
	virtual bool ResolveLinkPartner(const FGSTN_EntityRef& Partner, FGSTN_LinkPartnerView& OutView) const override;

	void RegisterObject(UGSTN_HeldObjectComponent& Object);
	void UnregisterObject(UGSTN_HeldObjectComponent& Object);

	/** Host only: Holder asks to pick up the nearest object; resolved after the next fixed frame, against every other request of it */
	void RequestPickup(AActor* Holder);

	/** Host only: Holder puts its object down (the grab button again); done after the next fixed frame. Nothing held: nothing happens */
	void RequestDrop(AActor* Holder);

	/** The object Holder holds, on any machine (from the replicated state); null if none */
	UGSTN_HeldObjectComponent* FindHeldBy(const AActor* Holder) const;

	/**
	 * The object a pickup from From would take, on any machine, from the replicated object state (FGSTN_HeldRules::FindPickupTarget with the
	 * same candidates the host races). Read by the held tick extension for Interact.Target.Pickup and by the prompt. Interim (plan G2): the
	 * object state is the host's, not stamped records, so around a pickup or a landing a client may project the target a round trip early or
	 * late; the pickup itself stays the host's decision. Null when nothing is in reach
	 */
	UGSTN_HeldObjectComponent* FindPickupTarget(const FVector& From) const;

	/** Host only: the throw ability fired the object Holder held as Shot; the object flies with it */
	void NoteThrown(AActor* Holder, AGSTN_Projectile& Shot);

	/**
	 * Any machine (M10 G9): the enemy Holder lifts, from Holder's Body link state Body: role 1 of a live link of a type in the settings'
	 * LiftableLinkTypes (the grab), the partner a fighter that FGSTN_EnemyLiftRules lets Holder lift (its Weight attribute and Combat.Held.NoLift
	 * against Holder's capacity). Reads static data of the two fighters only (attributes and kit tags), so the held tick extension projects the
	 * same Combat.Held.CarryingEnemy on every machine. OutLoadRatio = enemy weight over the capacity. Null when none
	 */
	static AActor* FindLiftedEnemy(const AActor* Holder, const FGSTN_LinkState& Body, float* OutLoadRatio = nullptr, FString* OutWhyNot = nullptr);
	/** The same from Holder's latest combat sync state (the throw ability) */
	static AActor* FindLiftedEnemy(const AActor* Holder, float* OutLoadRatio = nullptr, FString* OutWhyNot = nullptr);

	/**
	 * Host only (M10 G9): the throw ability fired Holder's lifted enemy as Shot (its hitbox, credited to Holder). The grab ends on both sides
	 * (LinkCommand Break, reason Throw, each stamped with its side's latest frame, as the watchdog posts them) and the enemy leaves the parked
	 * grab where it is with the shot's velocity, its regrab lock set; the shot already counts it as hit, so it never hits the body it stands for
	 */
	void NoteEnemyThrown(AActor* Holder, AGSTN_Projectile& Shot);

	/**
	 * Throwing client only (M10 G8): the throw just drew Ghost; the object Holder holds is drawn at it until the host's projectile takes over
	 * (adopted: the object follows it with no jump) or the ghost goes unmatched (the object goes back to what the host says: still in hand)
	 */
	void NotePredictedThrow(AActor* Holder, AGSTN_Projectile& Ghost);

	/** What became of this client's object ghosts (harness, gstn.Held.GhostReport) */
	struct FGhostStats
	{
		int32 Predicted = 0;
		int32 Handed = 0;
		int32 Snapped = 0;
		int32 Expired = 0;
		/** The host's projectile against the ghost at the handover frame (the projectile plugin's reconcile error), last and worst handed */
		double LastErrorCm = 0.0;
		double MaxHandedErrorCm = 0.0;
	};
	const FGhostStats& GetGhostStats() const { return GhostStats; }
	/** Client: Object's ghost went without a host projectile taking over */
	void NoteGhostExpired(const UGSTN_HeldObjectComponent& Object);

	bool HasAuthority() const;

	/** The last fixed frame seen on this machine, INDEX_NONE before the first */
	int32 GetLastFrame() const { return LastFrame; }

	const TArray<TWeakObjectPtr<UGSTN_HeldObjectComponent>>& GetObjects() const { return Objects; }

	/** Public for the dev commands: one log line per object (state, holder, location), "HeldReport ..." */
	void LogReport() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating);

	/** gstn.Held.Debug 1 (M10 G12, not in Shipping): draws this world's objects, holders and flights for one fixed frame */
	void DrawDebugFrame(int32 Frame) const;
	void ResolvePickups(int32 Frame);
	void StepObjects(int32 Frame);
	/** Host: the drops asked for and the breaks of the frame (requests, holder tags, holder events) */
	void StepHolds(int32 Frame);
	/** Host: Object stops being held for Reason: placed in front of its holder (or where it is without one) and the holder's hand emptied */
	void EndHold(UGSTN_HeldObjectComponent& Object, EGSTN_HeldBreakReason Reason, int32 Frame, const FString& Detail = FString());
	/**
	 * Host: posts the start (bHold: a Form of the Held Hand link, its serial kept on the object) or the end (a Break of that serial with Reason)
	 * of Object's hold to Holder's combat inbox, stamped with the holder's latest frame
	 */
	void PostHoldCommand(AActor* Holder, UGSTN_HeldObjectComponent& Object, bool bHold, EGSTN_HeldBreakReason Reason);
	void HandleSimEvent(const FGSTN_SimEvent& Event);
	/** The point on the floor below Location (WorldStatic), raised by HalfHeight; Location itself without a floor within 50 m */
	FVector FloorBelow(const FVector& Location, float HalfHeight, const AActor* Ignore) const;
	/** The holder's MaxCarryWeight attribute (0 when it has none: FGSTN_HeldLoad::EffectiveCapacity then uses the settings' default) */
	static float CarryCapacityOf(const AActor* Holder);
	/** Host (M10 G7): the object breaks (hidden until its respawn frame) */
	void BreakObject(UGSTN_HeldObjectComponent& Object, int32 Frame, const FString& On);
	/**
	 * Host (M10 G7, slice 8): the object explodes at At: a one-frame shot of ExplodeRadiusCm that pushes away from At (every fighter in it,
	 * through the projectile's hit pipeline, credited to the thrower), then the object breaks
	 */
	void ExplodeObject(UGSTN_HeldObjectComponent& Object, const FVector& At, int32 Frame, const FString& On);
	/** Client (M10 G8): a ghost met the host's projectile; an object drawn at the ghost follows the host's one */
	void HandleGhostReconciled(AGSTN_Projectile* Ghost, AGSTN_Projectile* Authority, bool bAdopted);
	/** Host (M10 G7): the object rebounds as a new shot from From with Velocity (the thrower's credit). False = it could not fire: it lands */
	bool Rebound(UGSTN_HeldObjectComponent& Object, const FVector& From, const FVector& Velocity, int32 Frame, const TArray<int32>& ThrowHitTargets);

	struct FQueuedPickup
	{
		TWeakObjectPtr<AActor> Holder;
		int32 Frame = INDEX_NONE;
	};

	TArray<TWeakObjectPtr<UGSTN_HeldObjectComponent>> Objects;
	TArray<FQueuedPickup> Pending;
	/** Holders that asked to drop, and holders an event of the frame broke (with the event type), served by StepHolds */
	TArray<TWeakObjectPtr<AActor>> PendingDrops;
	TArray<TPair<TWeakObjectPtr<AActor>, FGameplayTag>> PendingEventBreaks;
	FGSTN_SimEventListenerHandle EventListener;
	int32 NextObjectId = 0;
	/** Host: the last Hand link serial formed per holder, so two holds formed before the first is consumed never share a serial */
	TMap<TWeakObjectPtr<AActor>, uint8> LastHandSerial;
	int32 LastFrame = INDEX_NONE;
	FDelegateHandle PostFixedTickHandle;
	FDelegateHandle GhostReconciledHandle;
	FGhostStats GhostStats;
};
