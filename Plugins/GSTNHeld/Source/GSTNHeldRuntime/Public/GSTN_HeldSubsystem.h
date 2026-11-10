// Held objects, world side (needs the GAS-NPP fork for the fixed tick it reads).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_HeldTypes.h"
#include "Events/GSTN_SimEventSubsystem.h"
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
 * The link (slice 2): every start and end of a hold is also posted to the holder's combat inbox (FGSTN_HeldInbox, stamped with the holder's
 * frame), so the holder's FGSTN_HeldFragment and its projected Combat.Held.Carrying are simulation state: predicted, rolled back, the same on
 * every machine for a frame. The object half stays the host's: pickups and throws are host side effects of the abilities (like a projectile's
 * fire), and a client only reads the replicated object state to draw and to build its ghost. GSTNLink's Hand channel is fighter-to-fighter
 * only today; when it takes an object partner (M6 neutral hooks) the fragment folds into it and the rules here stay.
 */
UCLASS()
class GSTNHELDRUNTIME_API UGSTN_HeldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_HeldSubsystem* Get(const UWorld* World);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterObject(UGSTN_HeldObjectComponent& Object);
	void UnregisterObject(UGSTN_HeldObjectComponent& Object);

	/** Host only: Holder asks to pick up the nearest object; resolved after the next fixed frame, against every other request of it */
	void RequestPickup(AActor* Holder);

	/** Host only: Holder puts its object down (the grab button again); done after the next fixed frame. Nothing held: nothing happens */
	void RequestDrop(AActor* Holder);

	/** The object Holder holds, on any machine (from the replicated state); null if none */
	UGSTN_HeldObjectComponent* FindHeldBy(const AActor* Holder) const;

	/** Host only: the throw ability fired the object Holder held as Shot; the object flies with it */
	void NoteThrown(AActor* Holder, AGSTN_Projectile& Shot);

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
	void ResolvePickups(int32 Frame);
	void StepObjects(int32 Frame);
	/** Host: the drops asked for and the breaks of the frame (requests, holder tags, holder events) */
	void StepHolds(int32 Frame);
	/** Host: Object stops being held for Reason: placed in front of its holder (or where it is without one) and the holder's hand emptied */
	void EndHold(UGSTN_HeldObjectComponent& Object, EGSTN_HeldBreakReason Reason, int32 Frame, const FString& Detail = FString());
	/** Host: posts the start (bHold) or the end of Object's hold to Holder's combat inbox, stamped with the holder's latest frame */
	void PostHoldCommand(AActor* Holder, const UGSTN_HeldObjectComponent& Object, bool bHold) const;
	void HandleSimEvent(const FGSTN_SimEvent& Event);
	/** The point on the floor below Location (WorldStatic), raised by HalfHeight; Location itself without a floor within 50 m */
	FVector FloorBelow(const FVector& Location, float HalfHeight, const AActor* Ignore) const;

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
	int32 LastFrame = INDEX_NONE;
	FDelegateHandle PostFixedTickHandle;
};
