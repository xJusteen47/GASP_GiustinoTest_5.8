// Projectile, world side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Movement/GSTN_BallisticMode.h"
#include "Movement/GSTN_ShotAim.h"

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
	 * Along the move stick of the fire frame's command turned onto the shooter's plane (camera relative, as the stick moves the character),
	 * raised by LaunchPitchDeg; the facing with the stick centred (M10: a tapped throw goes where the left stick points)
	 */
	Stick,
	/**
	 * At the Target actor of the triggering gameplay event (AI, traps, the harness), solved as a straight line or, with gravity, as
	 * the flat or high arc that passes through it. Never used to steer a player's shot: the player aims (precision tools, no auto-play)
	 */
	EventTarget,
	/**
	 * M11 L9 (owner, 2026-11-12: vertical aim only with the reticle on). With the reticle on in the fire frame's command
	 * (FGSTN_InputCmd::bPrecisionAim): along the camera aim of that command, up and down too, its pitch clamped to the ability's reticle
	 * pitch limits. Off: as ReticleOffAim (Facing or Stick), flat plus LaunchPitchDeg
	 */
	Reticle
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

	/** Stick: below this stick length (0..1) the stick counts as centred and the shot goes along the facing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StickAimDeadzone = 0.3f;

	/** EventTarget with gravity: the high arc (lob) instead of the flat one */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim")
	bool bHighArc = false;

	/** EventTarget: point aimed at, in the target's snapshot space */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (ForceUnits = "cm"))
	FVector TargetAimOffset = FVector::ZeroVector;

	/** Reticle with the reticle off: the flat aim used instead (Facing or Stick; anything else counts as Facing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (EditCondition = "Aim == EGSTN_ProjectileAim::Reticle"))
	EGSTN_ProjectileAim ReticleOffAim = EGSTN_ProjectileAim::Facing;

	/** Reticle with the reticle on: the lowest the shot may point (negative = down) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (EditCondition = "Aim == EGSTN_ProjectileAim::Reticle", ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float MinReticlePitchDeg = -60.f;

	/** Reticle with the reticle on: the highest the shot may point */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Aim", meta = (EditCondition = "Aim == EGSTN_ProjectileAim::Reticle", ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float MaxReticlePitchDeg = 60.f;

	/**
	 * How the shot reaches its target (M11 L9, per move): a flying projectile (the Projectile template) or a hitscan line (Hitscan below,
	 * nearest victim first). A hitscan keeps the template's hit data, source and MaxHits, and replaces its motion and shape
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Shot")
	EGSTN_ShotDelivery Delivery = EGSTN_ShotDelivery::Projectile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile|Shot", meta = (EditCondition = "Delivery == EGSTN_ShotDelivery::Hitscan"))
	FGSTN_HitscanSpec Hitscan;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	virtual void OnCombatFramesElapsed() override;

	/** Builds the shot and hands it to the host, or to the ghost on the predicting client. Whether a projectile was spawned (false on a client) */
	bool Fire();

	/** Fired or skipped: the ability is done */
	void FinishFire();

	/**
	 * Extension hook, before the origin and direction are computed: a child fills or changes the shot's template (speed, gravity, shape,
	 * hit data). bHost is false on the predicting client, which only draws a ghost of the result. Return false to skip the shot (nothing
	 * fired, no ghost). The default keeps the Projectile property as it is
	 */
	virtual bool PrepareShot(FGSTN_ProjectileParams& InOutParams, bool bHost) { return true; }

	/**
	 * Extension hook, asked once in ActivateAbility after the context is captured: true when the child fires later itself (a held button that aims
	 * until its release calls Fire and FinishFire from its own tick); FireDelayFrames is then not used. Must be the same on every machine (read
	 * the activation context, not the world). The default fires as usual
	 */
	virtual bool DeferFire() const { return false; }

	/**
	 * Extension hook: the aim of this shot, read once at the fire frame on every machine that builds it (host and ghost), so it must come from the
	 * ability's sync state or the frame's command, never a member changed at run time. The default is the Aim property
	 */
	virtual EGSTN_ProjectileAim GetShotAim() const { return Aim; }

	/** Extension hook, host only: the projectile this ability just spawned */
	virtual void OnShotFired(class AGSTN_Projectile& Shot) {}

	/**
	 * Extension hook: the elevation added to Facing, Stick and CommandAim for this shot, read at the fire frame on every machine that builds it
	 * (the same rule as GetShotAim). The default is LaunchPitchDeg
	 */
	virtual float GetShotLaunchPitchDeg() const { return LaunchPitchDeg; }

	/**
	 * Extension hook, predicting client only: the ghost just drawn for this shot (also when a re-simulated fire finds the one already drawn).
	 * Presentation only: a thrown object follows it (M10 G8)
	 */
	virtual void OnGhostSpawned(class AGSTN_Projectile& Ghost) {}

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

/**
 * Starter aimed move (M11 L10, the first content of L9): a rifle shot that is a hitscan line along the camera aim while the reticle is on
 * (pitch clamped to the reticle limits) and along the facing, flat, with it off. A kit binds it to the strong commands with
 * Input.Context.PrecisionAim required, so the same button keeps its plain move without the reticle. Every number is a placeholder for the
 * playtest; a Blueprint child (or this class's defaults) retunes it. No recovery of its own: it ends on the fire frame, so the press rate
 * is the fire rate (the binding's buffer and the press edge, not a cooldown)
 */
UCLASS(Blueprintable)
class GSTNPROJECTILENP_API UGSTN_GA_AimedHitscanShot : public UGSTN_GA_FireProjectile
{
	GENERATED_BODY()

public:
	UGSTN_GA_AimedHitscanShot(const FObjectInitializer& ObjectInitializer);
};
