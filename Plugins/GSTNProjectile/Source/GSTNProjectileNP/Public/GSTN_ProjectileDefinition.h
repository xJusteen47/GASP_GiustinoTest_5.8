// Projectile, world side (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_MoveActions.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Movement/GSTN_BallisticMode.h"
#include "Movement/GSTN_ShotAim.h"

#include "GSTN_ProjectileDefinition.generated.h"

class AGSTN_Projectile;
class UGSTN_GA_Base;
class UGSTN_GA_BaseCombat;

/** Where the shot goes (moved here from the fire ability with CP2: the shot spec of a definition uses it too) */
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
 * Everything a shot is made of (CP2 of the Blueprint seams): the template, where it leaves from and where it goes. The same fields as the fire
 * ability's (UGSTN_GA_FireProjectile builds one from its own properties), so a move fires the shot of a data asset and the fire ability its own,
 * through one builder (FGSTN_ProjectileShot)
 */
USTRUCT(BlueprintType)
struct GSTNPROJECTILENP_API FGSTN_ProjectileShotSpec
{
	GENERATED_BODY()

	/** The shot's parameters. Origin, Direction and OwnerEntityId are filled at fire time; GravityAccel zero = straight, non-zero = arc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	FGSTN_ProjectileParams Projectile;

	/** Muzzle in the owner's snapshot space (X forward, Z up, from the capsule centre) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ForceUnits = "cm"))
	FVector MuzzleOffset = FVector(80.0, 0.0, 30.0);

	/** A flying projectile (the template) or a hitscan line (Hitscan below, nearest victim first) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	EGSTN_ShotDelivery Delivery = EGSTN_ShotDelivery::Projectile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (EditCondition = "Delivery == EGSTN_ShotDelivery::Hitscan"))
	FGSTN_HitscanSpec Hitscan;

	/** Where the shot goes (EventTarget needs a target the move knows; from a move without one it goes along the facing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	EGSTN_ProjectileAim Aim = EGSTN_ProjectileAim::Facing;

	/** Elevation added to Facing, Stick and CommandAim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float LaunchPitchDeg = 0.f;

	/** Stick: below this stick length (0..1) the stick counts as centred and the shot goes along the facing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StickAimDeadzone = 0.3f;

	/** EventTarget with gravity: the high arc (lob) instead of the flat one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	bool bHighArc = false;

	/** EventTarget: point aimed at, in the target's snapshot space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ForceUnits = "cm"))
	FVector TargetAimOffset = FVector::ZeroVector;

	/** Reticle with the reticle off: the flat aim used instead (Facing or Stick; anything else counts as Facing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	EGSTN_ProjectileAim ReticleOffAim = EGSTN_ProjectileAim::Facing;

	/** Reticle with the reticle on: the lowest the shot may point (negative = down) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float MinReticlePitchDeg = -60.f;

	/** Reticle with the reticle on: the highest the shot may point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float MaxReticlePitchDeg = 60.f;
};

/** What a move changes on a shot when it fires it: the second shot of a double press at another angle, faster or slower */
USTRUCT(BlueprintType)
struct GSTNPROJECTILENP_API FGSTN_ProjectileShotAdjust
{
	GENERATED_BODY()

	/** Turned around the shooter's up axis after the aim (positive = to the right) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adjust", meta = (ClampMin = "-180.0", ClampMax = "180.0", ForceUnits = "deg"))
	float YawOffsetDeg = 0.f;

	/** Raised after the aim (negative = down), clamped short of straight up or down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adjust", meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float PitchOffsetDeg = 0.f;

	/** Multiplies the template's speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adjust", meta = (ClampMin = "0.01"))
	float SpeedScale = 1.f;

	bool IsIdentity() const { return FMath::IsNearlyZero(YawOffsetDeg) && FMath::IsNearlyZero(PitchOffsetDeg) && FMath::IsNearlyEqual(SpeedScale, 1.f); }
};

/**
 * A projectile a move fires (CP2 of the Blueprint seams): one asset per kind of shot (a fireball, a knife, a rifle line), named by the Fire
 * Projectile move action on a timeline key or by the Fire Move Projectile node of a move's graph. Data only: the shot runs through the same
 * host-spawn-once path as UGSTN_GA_FireProjectile, and the predicting client draws its ghost
 */
UCLASS(BlueprintType)
class GSTNPROJECTILENP_API UGSTN_ProjectileDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ShowOnlyInnerProperties))
	FGSTN_ProjectileShotSpec Shot;
};

/** Builds a shot for an ability's avatar and hands it to the host, or draws its ghost on the predicting client */
struct GSTNPROJECTILENP_API FGSTN_ProjectileShot
{
	struct FResult
	{
		/** The host's projectile (host only) */
		AGSTN_Projectile* Shot = nullptr;

		/** The ghost drawn on the predicting client (also the one already drawn when a re-simulated fire finds it) */
		AGSTN_Projectile* Ghost = nullptr;
	};

	/**
	 * Fires Spec for Ability's avatar (determinism, spawn once and ghost: see UGSTN_GA_FireProjectile). TargetEntityId is the EventTarget's combat
	 * entity, INDEX_NONE without one. Prepare may change the template first (bHost false on the predicting client); false skips the shot. Nothing
	 * on a client that does not control the avatar. One ghost per owner and frame: two shots of one owner in the same frame draw one ghost
	 */
	static FResult Fire(const FGSTN_ProjectileShotSpec& Spec, const UGSTN_GA_Base& Ability, int32 TargetEntityId, const FGSTN_ProjectileShotAdjust& Adjust,
		TFunctionRef<bool(FGSTN_ProjectileParams& /*InOutParams*/, bool /*bHost*/)> Prepare);
};

/**
 * Move action: fires the projectile of a definition (CP2). Put it in the Actions of an Event key ("Fire" on frame N) for a data-only shot, or
 * run it from a graph with Fire Move Projectile
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Fire Projectile"))
struct GSTNPROJECTILENP_API FGSTN_MoveAction_FireProjectile : public FGSTN_MoveAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	TObjectPtr<UGSTN_ProjectileDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
	FGSTN_ProjectileShotAdjust Adjust;

	virtual bool Execute(const FGSTN_MoveActionContext& Context) const override;
	virtual FText Validate() const override;
};

/** The projectile nodes of a combat move's graph (CP2) */
UCLASS()
class GSTNPROJECTILENP_API UGSTN_ProjectileMoveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Fires the projectile of Definition from this move (call it from On Move Key, On Move Hit Confirmed or another simulation event of the move).
	 * True when the shot was made here: the host spawned it, or the predicting client drew its ghost. Does nothing outside the simulation step of
	 * an active move. Adjust turns or speeds up this one shot
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Move Actions", meta = (DefaultToSelf = "Move", HidePin = "Move", DisplayName = "Fire Move Projectile"))
	static bool FireMoveProjectile(UGSTN_GA_BaseCombat* Move, UGSTN_ProjectileDefinition* Definition, FGSTN_ProjectileShotAdjust Adjust);
};
