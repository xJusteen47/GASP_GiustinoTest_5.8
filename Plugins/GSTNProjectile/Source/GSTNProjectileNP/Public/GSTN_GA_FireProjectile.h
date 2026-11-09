// Projectile, world side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Movement/GSTN_BallisticMode.h"

#include "GSTN_GA_FireProjectile.generated.h"

/** Where the shot goes */
UENUM(BlueprintType)
enum class EGSTN_ProjectileAim : uint8
{
	/** Along the shooter's facing, raised by LaunchPitchDeg */
	Facing,
	/** Along the camera aim of the activating command (GetAimRotation), raised by LaunchPitchDeg; the facing without an aim */
	CommandAim,
	/**
	 * At the Target actor of the triggering gameplay event (AI, traps, the harness), solved as a straight line or, with gravity, as
	 * the flat or high arc that passes through it. Never used to steer a player's shot: the player aims (precision tools, no auto-play)
	 */
	EventTarget
};

/**
 * The ability side of a projectile (entita-dinamiche review §3): an ability that fires one projectile through the host's
 * UGSTN_ProjectileSubsystem::FireProjectile after FireDelayFrames combat frames (hitstop does not count), then ends.
 *
 * Determinism and spawn-once: the ability runs predicted like every combat ability (the wait is a sync variable of UGSTN_GA_Base), but
 * the fire itself is a host side effect: on a client the call is skipped (CanFire is false), on the host it runs once because the host
 * never resimulates. The shot starts from the owner's combat snapshot transform (the pose hits are judged against), not from a Mover
 * query. The projectile's own frames are a closed form, so nothing of it becomes ability state. On the client that predicts the ability
 * the same shot is built from its own snapshot and drawn at once as a ghost (UGSTN_ProjectileSubsystem::SpawnGhost, slice 3), so the
 * shooter does not see the shot one round trip late; the host's projectile adopts the ghost when it arrives.
 *
 * Blueprint children set the template, the delay and the aim; a timeline track key will replace FireDelayFrames for moves later.
 */
UCLASS(Blueprintable)
class GSTNPROJECTILENP_API UGSTN_GA_FireProjectile : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_FireProjectile(const FObjectInitializer& ObjectInitializer);

	/** The shot's parameters. Origin, Direction and OwnerEntityId are filled at fire time; GravityAccel zero = straight, non-zero = arc */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Shot")
	FGSTN_ProjectileParams Projectile;

	/** Combat frames from the activation to the fire (0 = in the activation frame) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Shot", meta = (ClampMin = "0"))
	int32 FireDelayFrames = 0;

	/** Muzzle in the owner's snapshot space (X forward, Z up, from the capsule centre) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Shot", meta = (ForceUnits = "cm"))
	FVector MuzzleOffset = FVector(80.0, 0.0, 30.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim")
	EGSTN_ProjectileAim Aim = EGSTN_ProjectileAim::Facing;

	/** Elevation added to Facing and CommandAim (a lob without a target); ignored by EventTarget */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float LaunchPitchDeg = 0.f;

	/** EventTarget with gravity: the high arc (lob) instead of the flat one */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim")
	bool bHighArc = false;

	/** EventTarget: point aimed at, in the target's snapshot space */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (ForceUnits = "cm"))
	FVector TargetAimOffset = FVector::ZeroVector;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void OnCombatFramesElapsed() override;

	/** Builds the shot and hands it to the host, or to the ghost on the predicting client. Whether a projectile was spawned (false on a client) */
	bool Fire();

	/** Fired or skipped: the ability is done */
	void FinishFire();

private:
	/** Combat entity id of the EventTarget's actor, read once at activation; INDEX_NONE without a target */
	UPROPERTY(Transient)
	FSyncInteger TargetEntityId;
};

/**
 * The same ability with a ServerOnly execution policy: for shooters nobody predicts (AI, traps, a dev command, the harness). Activating
 * it on the host does not desync the owning client, since the client never runs it.
 */
UCLASS(Blueprintable)
class GSTNPROJECTILENP_API UGSTN_GA_FireProjectileServer : public UGSTN_GA_FireProjectile
{
	GENERATED_BODY()

public:
	UGSTN_GA_FireProjectileServer(const FObjectInitializer& ObjectInitializer);
};
