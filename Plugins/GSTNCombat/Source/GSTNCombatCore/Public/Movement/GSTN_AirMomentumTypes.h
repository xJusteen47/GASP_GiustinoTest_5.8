// Pure combat logic (no GAS, no Network Prediction): the air momentum model (owner 2026-11-18, docs/notes/handovers/air-momentum-1.md).
//
// The horizontal velocity in the air is split in two channels:
// - the PLAYER channel: what the stick built (the takeoff run, the air drift). The stick accelerates it up to the air top speed, a stick
//   against it brakes it, a neutral stick lets it decay at the style's neutral deceleration (Astro: fast, Mario: slow);
// - the EXTERNAL channel: what something else gave (a knockback, a trampoline, a bounce, any FLayeredMove_Launch child). It decays
//   slowly whatever the stick does; only a stick held against it brakes it, a stick along it adds nothing (no speed past the cap).
// The total is their sum. Only the external channel is state (Mover's sync state); the player channel is the total minus it.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_AirMomentumTypes.generated.h"

/** How the player channel behaves with a neutral stick in the air (owner 2026-11-18: both, per kit; default less inertia) */
UENUM(BlueprintType)
enum class EGSTN_AirInertiaStyle : uint8
{
	/** Astro Bot-like (default): a released stick stops the drift almost at once, the character falls nearly straight */
	HighFriction,
	/** Mario-like: a released stick keeps most of the drift (the old feel, a little stronger braking than Mover's 200 cm/s2) */
	Carry,
	/** The model off: Mover's falling mode as before (one channel, FallingDeceleration, AirControlPercentage x ground acceleration) */
	MoverLegacy UMETA(DisplayName = "Mover (Legacy)")
};

/**
 * Air momentum tuning of a kit (part of the locomotion profile, FGSTN_RunAirTuning::Momentum). Accelerations in cm/s2, at the sim rate
 * of 62.5 Hz one frame is 16 ms: 1000 cm/s2 changes the speed by 16 cm/s per frame. The player-side accelerations are multiplied by
 * the air control of the frame (AirControl stat x takeoff stage AirControlScale, or a move's own air control), so the existing per-stage
 * and per-move steering keeps working.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_AirMomentumTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (DisplayName = "Inertia Style"))
	EGSTN_AirInertiaStyle Style = EGSTN_AirInertiaStyle::HighFriction;

	/** Player channel: acceleration toward the stick, up to the air top speed (x air control). Mover today gives ~0.85 x 1800 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float AirAcceleration = 3200.f;

	/**
	 * Player channel: acceleration with the stick at 90 degrees to the drift (x air control). The rate follows the dot between the stick and
	 * the drift continuously (owner 2026-11-18): dot 1 = AirAcceleration, dot 0 = this, dot -1 = AirBrake, linear in between
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float AirAccelerationSide = 3200.f;

	/** Player channel: deceleration when the stick points straight against the drift, dot -1 (x air control) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float AirBrake = 4500.f;

	/** HighFriction: neutral stick deceleration of the player channel. 4000 stops a 450 cm/s drift in ~7 frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (EditCondition = "Style == EGSTN_AirInertiaStyle::HighFriction", ClampMin = "0", ForceUnits = "cm/s^2"))
	float HighFrictionNeutralDeceleration = 4000.f;

	/** Carry: neutral stick deceleration of the player channel. ~550 = Mario's 0.12 u/frame2; Mover's old value was 200 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (EditCondition = "Style == EGSTN_AirInertiaStyle::Carry", ClampMin = "0", ForceUnits = "cm/s^2"))
	float CarryNeutralDeceleration = 550.f;

	/** External channel: decay whatever the stick does (springs and knockback keep their arc) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|External Push", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float ExternalDeceleration = 300.f;

	/** External channel: extra deceleration with the stick fully against it (scaled by how much it points against, x air control) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|External Push", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float ExternalBrake = 1800.f;

	/** Under this speed the external channel is dropped (it becomes the player's) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|External Push", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ExternalMinSpeed = 20.f;

	/**
	 * Drift DI (owner 2026-11-18: "the air control during hitstun"): while a knockback flies under hitstun (movement locked) the victim keeps
	 * its air control, scaled by DriftDIControlScale (acceleration, brake and top speed of the player channel). It adds to the knockback: the
	 * external channel is never braked by it, it only decays. The launch DI (FGSTN_DIParams, hit frame only) is separate and unchanged.
	 * Default 1 = full air control, as Smash Ultimate is reported to allow during hitstun: UNVERIFIED. 0 = no drift (the old behaviour)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Drift DI", meta = (ClampMin = "0", ClampMax = "2"))
	float DriftDIControlScale = 1.f;

	bool IsEnabled() const { return Style != EGSTN_AirInertiaStyle::MoverLegacy; }
	/** The player channel's rate for the dot between the stick direction and the drift direction (1 along, 0 sideways, -1 against) */
	float GetSteerRate(float Dot) const
	{
		const float D = FMath::Clamp(Dot, -1.f, 1.f);
		return D >= 0.f ? FMath::Lerp(AirAccelerationSide, AirAcceleration, D) : FMath::Lerp(AirAccelerationSide, AirBrake, -D);
	}

	float GetNeutralDeceleration() const { return Style == EGSTN_AirInertiaStyle::Carry ? CarryNeutralDeceleration : HighFrictionNeutralDeceleration; }
};

/** One air frame for the model. Vectors are horizontal (on the movement plane), world space */
struct GSTNCOMBATCORE_API FGSTN_AirMomentumInput
{
	/** Horizontal velocity at the start of the frame (both channels) */
	FVector Velocity = FVector::ZeroVector;
	/** External channel at the start of the frame */
	FVector External = FVector::ZeroVector;
	/** Stick on the movement plane, magnitude 0..1. Zero under a movement lock */
	FVector Stick = FVector::ZeroVector;
	/** Air top speed of the player channel (TakeoffCap or Mover's MaxSpeed), cm/s */
	float TopSpeed = 0.f;
	/** Air control of the frame, 0..1 (scales the player-side accelerations and the external brake) */
	float Control = 1.f;
	/** Extra per-frame fraction of the external channel lost (a knockback move's own drag, FGSTN_LayeredMove_Knockback::DecayPerFrame) */
	float ExternalDragPerFrame = 0.f;
	float DeltaSeconds = 0.016f;
	/** Hitstun drift DI frame: the stick steers the player channel only (Control and TopSpeed already scaled), the external channel just decays */
	bool bDriftDI = false;
};

struct GSTNCOMBATCORE_API FGSTN_AirMomentumOutput
{
	/** New horizontal velocity (both channels) */
	FVector Velocity = FVector::ZeroVector;
	/** New external channel */
	FVector External = FVector::ZeroVector;
};

/** The model, pure: same input, same output on every machine */
struct GSTNCOMBATCORE_API FGSTN_AirMomentumLogic
{
	static FGSTN_AirMomentumOutput Step(const FGSTN_AirMomentumTuning& Tuning, const FGSTN_AirMomentumInput& Input);

	/**
	 * The external channel after Mover moved the character: what a collision took from the proposed velocity (Proposed - Moved, a wall) is
	 * taken from the push too, and the push is never longer than the moved velocity nor against it. MinSpeed drops a tiny remainder
	 */
	static FVector ClampExternalToMoved(const FVector& External, const FVector& ProposedHorizontal, const FVector& MovedHorizontal, float MinSpeed);
};
