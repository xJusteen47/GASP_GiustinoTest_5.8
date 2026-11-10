// Held objects, world side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "GSTN_GA_FireProjectile.h"

#include "GSTN_HeldAbilities.generated.h"

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
 * Throw the held object: a GSTNProjectile shot whose template is the object's throw tuning (speed, gravity, lifetime, hit shape and hit
 * data), fired by the projectile ability path, so the host sweep, PostHit into the victim's inbox, the credit to the thrower (AttackerId)
 * and the predicted ghost on the throwing client are the projectile plugin's, unchanged. On the host the object then flies with the
 * shot (UGSTN_HeldSubsystem::NoteThrown). Nothing held: no shot, no ghost. Aim, launch pitch and delay are the fire ability's fields
 * (TEST VALUES here); the plan's aim policy (stick or camera aim while aiming) comes with a later slice.
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldThrow : public UGSTN_GA_FireProjectile
{
	GENERATED_BODY()

public:
	UGSTN_GA_HeldThrow(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool PrepareShot(FGSTN_ProjectileParams& InOutParams, bool bHost) override;
	virtual void OnShotFired(AGSTN_Projectile& Shot) override;
};

/** The same, ServerOnly: for the harness, AI and dev commands */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GA_HeldThrowServer : public UGSTN_GA_HeldThrow
{
	GENERATED_BODY()

public:
	UGSTN_GA_HeldThrowServer(const FObjectInitializer& ObjectInitializer);
};
