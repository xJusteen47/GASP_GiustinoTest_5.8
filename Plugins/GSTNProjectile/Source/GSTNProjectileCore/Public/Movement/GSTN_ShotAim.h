// Projectile, pure part (engine + GSTNCombatCore only).

#pragma once

#include "CoreMinimal.h"
#include "Movement/GSTN_BallisticMode.h"

#include "GSTN_ShotAim.generated.h"

/** How a shot reaches what it hits (M11 L9: data on the move, owner 2026-11-12) */
UENUM(BlueprintType)
enum class EGSTN_ShotDelivery : uint8
{
	/** A projectile that flies (FGSTN_ProjectileParams: speed, gravity, lifetime) */
	Projectile,
	/**
	 * Hits along the whole line at once: a projectile that does not move, whose shape is a capsule from the muzzle to Range (FGSTN_ShotAim::
	 * MakeHitscan). It resolves on the host on the frame after the fire, through the same hit pipeline, nearest victim first
	 */
	Hitscan
};

/** The line of a hitscan shot */
USTRUCT(BlueprintType)
struct GSTNPROJECTILECORE_API FGSTN_HitscanSpec
{
	GENERATED_BODY()

	/** How far the line reaches from the muzzle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Hitscan", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float RangeCm = 5000.f;

	/** Thickness of the line: 0 would be a ray; a few cm forgive a shot that grazes a hurtbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Hitscan", meta = (ClampMin = "0.1", ForceUnits = "cm"))
	float RadiusCm = 5.f;
};

/**
 * The aim and delivery math of an aimed shot (M11 L9), kept pure so the host and the predicting client build the same shot from the same
 * command and a test can check it without a world.
 */
struct GSTNPROJECTILECORE_API FGSTN_ShotAim
{
	/**
	 * Speed of a hitscan carrier: it moves under 2 cm in a frame, so the capsule stays where it was fired, and its velocity still keeps the
	 * direction through the segment's 1/100 quantization (the carrier faces along the velocity)
	 */
	static constexpr float HitscanCarrierSpeed = 100.f;

	/**
	 * Turns InOut (origin and direction already set) into a hitscan: no gravity, one frame of life, nearest victim first, and a capsule
	 * along the carrier's X from the muzzle to Spec.RangeCm. Speed, gravity, lifetime and shape of the template are replaced; the hit data,
	 * source tag and MaxHits (more than 1 pierces, nearest first) are kept
	 */
	static void MakeHitscan(FGSTN_ProjectileParams& InOut, const FGSTN_HitscanSpec& Spec);

	/**
	 * The direction of a reticle shot (EGSTN_ProjectileAim::Reticle). Owner, 2026-11-12: vertical aim only with the reticle on. On: along
	 * ReticleAim (the command's camera aim) with its pitch clamped to [MinPitchDeg, MaxPitchDeg], or FlatDirection when the command has no
	 * aim. Off: FlatDirection (facing or stick) raised by LaunchPitchDeg, as every non-reticle shot
	 */
	static FVector ReticleDirection(bool bReticleOn, const FRotator& ReticleAim, const FVector& FlatDirection, float MinPitchDeg, float MaxPitchDeg, float LaunchPitchDeg);
};
