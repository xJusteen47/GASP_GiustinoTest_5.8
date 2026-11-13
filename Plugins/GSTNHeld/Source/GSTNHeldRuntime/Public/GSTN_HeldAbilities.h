// Held objects, world side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "GSTN_GA_FireProjectile.h"

#include "GSTN_HeldAbilities.generated.h"

struct FGSTN_HeldThrowWindup;

/**
 * Pick up the nearest object (M10 slice 1). The ability runs predicted like every combat ability and ends at once; its only effect is a
 * host side effect: the request goes to UGSTN_HeldSubsystem, which resolves it with every other request of that frame (earliest frame,
 * then lower entity id) and makes the object Held. A client never changes the object: it sees the hold one round trip later. Requires
 * nothing yet: the plan's gate (Interact.Target.Pickup, not carrying) comes with the interact probe (G2) and the Hand link (G1).
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldPickup : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** The same, ServerOnly: for the harness, AI and dev commands (no client desync on a host-only activation) */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldPickupServer : public UGSTN_GA_HeldPickup
{
	GENERATED_BODY()

public:
	UGSTN_GA_HeldPickupServer(const FObjectInitializer& ObjectInitializer);
};

/**
 * Put the held object down (M10 slice 2: the grab button again, Command.Held.Drop, binding requires Combat.Held.Carrying). Like the pickup it
 * ends at once and its only effect is a host side effect: UGSTN_HeldSubsystem::RequestDrop places the object in front of the holder after the
 * frame and ends the holder's Hand link (FGSTN_HeldLinkType), which the client gets by correction.
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldDrop : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

/** The same, ServerOnly: for the harness, AI and dev commands */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldDropServer : public UGSTN_GA_HeldDrop
{
	GENERATED_BODY()

public:
	UGSTN_GA_HeldDropServer(const FObjectInitializer& ObjectInitializer);
};

/**
 * Throw the held object: a GSTNProjectile shot whose template is the object's throw tuning (speed, gravity, lifetime, hit shape and hit
 * data), fired by the projectile ability path, so the host sweep, PostHit into the victim's inbox, the credit to the thrower (AttackerId)
 * and the predicted ghost on the throwing client are the projectile plugin's, unchanged. On the host the object then flies with the
 * shot (UGSTN_HeldSubsystem::NoteThrown). Nothing held: no shot, no ghost.
 *
 * Started by a button (Command.Held.Throw, the strong button by default) it fires on the button's release (owner's design 2026-11-10, TEST
 * VALUES in UGSTN_HeldRules::ThrowInput): released before AimHoldFrames it is a tap and goes along the left stick of the release frame
 * (the facing with the stick centred); held longer the reticle shows (UGSTN_PrecisionAimComponent::SetHoldAimShown, local) and the throw
 * goes along the camera of the release frame; with the reticle toggled on a tap aims with the camera too. Held MaxAimFrames, it goes on its
 * own. With AimView = Shoulder (slice 8, M10 G7) the held aim also asks the camera to go over the shoulder (AimViewRequestTags through the
 * precision aim component) and the throw goes along the converged aim plus ShoulderAimPitchDeg. On the throwing client the object is drawn at
 * the shot's ghost until the host's projectile takes over (slice 8, M10 G8). The press frame and the chosen aim are sync variables, so a re-simulation fires on the same frame the same way. Started without a
 * button (an event, the harness) it fires at once with the Aim property (EventTarget for the dev command).
 *
 * Wind-up (slice 10, owner's request 2026-11-11): from the release (or the activation without a button) the object stays in the hand for the
 * wind-up's ReleaseFrames (FGSTN_HeldRules::ResolveWindup: the object's own, the project's, or the thrown-enemy tuning) while its montage plays
 * on the predicted slot, then leaves: the shot, the ghost and the host's projectile start at that frame, aimed with that frame's stick or camera.
 * 0 frames = the throw fires in the release frame as before. The start clock and the length are sync variables. A holder that takes any of its
 * object's DropWhenHolderHasAny tags during the wind-up (a hit, a launch, a grab) stops it: no throw, the montage stops, and the host drops the
 * object as for any hit in hand.
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldThrow : public UGSTN_GA_FireProjectile
{
	GENERATED_BODY()

public:
	UGSTN_GA_HeldThrow(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

protected:
	virtual bool PrepareShot(FGSTN_ProjectileParams& InOutParams, bool bHost) override;
	virtual void OnShotFired(AGSTN_Projectile& Shot) override;
	virtual bool DeferFire() const override;
	virtual EGSTN_ProjectileAim GetShotAim() const override;
	virtual float GetShotLaunchPitchDeg() const override;
	virtual void OnGhostSpawned(AGSTN_Projectile& Ghost) override;

private:
	/** Local presentation: the reticle of a held aim on the pawn's own machine */
	void ShowAimReticle(bool bShown) const;

	/** Whether a button started this throw (it waits for the release); an event throws at once, after the wind-up */
	bool HasButton() const;

	/** The aim is chosen: fires now (instant wind-up) or starts the wind-up and its montage */
	void BeginWindup();

	/** One wind-up frame: stopped by a hit, or the object leaves the hand on the release frame */
	void TickWindup();

	/** The wind-up of what the avatar holds (null = nothing held: instant) */
	const FGSTN_HeldThrowWindup* FindWindup() const;

	/** The holder took a state that drops its object (the object's break rules, or the project's for a lifted enemy) */
	bool IsWindupInterrupted() const;

	/** Stops the wind-up montage if it is still the one on the synced slot */
	void StopWindupMontage();

	/** Host log of the wind-up for the harness: Phase = start, release or stop */
	void LogWindup(const TCHAR* Phase) const;

	/** Combat clock of the press; INDEX_NONE when not waiting for a release */
	UPROPERTY(Transient)
	FSyncInteger PressClock;

	/** EGSTN_HeldThrowAim chosen at the release */
	UPROPERTY(Transient)
	FSyncByte ReleaseAim;

	/** Combat clock of the wind-up's start; INDEX_NONE when not winding up */
	UPROPERTY(Transient)
	FSyncInteger WindupClock;

	/** The wind-up's ReleaseFrames, resolved at its start */
	UPROPERTY(Transient)
	FSyncInteger WindupFrames;

	/** The montage the wind-up plays, to find it on the slot when it stops. Presentation, not state */
	UPROPERTY(Transient)
	TObjectPtr<class UAnimMontage> WindupMontage;
};

/** The same, ServerOnly: for the harness, AI and dev commands */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldThrowServer : public UGSTN_GA_HeldThrow
{
	GENERATED_BODY()

public:
	UGSTN_GA_HeldThrowServer(const FObjectInitializer& ObjectInitializer);
};
