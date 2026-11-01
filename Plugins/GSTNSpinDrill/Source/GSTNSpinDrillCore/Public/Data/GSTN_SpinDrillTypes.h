// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GSTN_SpinDrillTypes.generated.h"

/**
 * Tuning groups of the spin and drill (spin-drill.md 6, with the owner answers of 13b). Every number the code needs between the
 * abilities lives here; frame data of a single ability (hitboxes, montage, cancel windows) stays in that ability's timeline.
 * UGSTN_SpinDrillTuning (GSTNSpinDrillGAS) groups these structs into one asset referenced by the kit.
 *
 * Simulation rule: the simulation reads these values copied into plain fields, never through a UObject at tick time. The only
 * object reference (the wobble curve) is presentation and is sampled once outside the simulation (UGSTN_DrillMath).
 */

/** Events that give the air spin back (spin-drill.md 6 "RefreshOn"). Several may happen on one frame. */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EGSTN_AirSpinRefresh : uint8
{
	None = 0 UMETA(Hidden),

	/** Touching walkable ground */
	Land = 1 << 0,

	/** Starting a wall slide or wall contact */
	Wall = 1 << 1,

	/** Being launched by a trampoline surface */
	Trampoline = 1 << 2,

	/** The drill's own launch out of a target (when the launch tuning allows it) or another launch */
	Launch = 1 << 3,

	/** Being hit */
	Hit = 1 << 4,
};
ENUM_CLASS_FLAGS(EGSTN_AirSpinRefresh);

/** Direction of the dive (spin-drill.md 6, 14 SD.6: gravity only in v1) */
UENUM(BlueprintType)
enum class EGSTN_DrillDiveDirection : uint8
{
	/** Along the entity's own gravity, whatever its direction */
	Gravity,

	/** Along the character's facing (later) */
	Facing,

	/** Along the stick (later) */
	Stick,
};

/** Direction of the drill launch out of a target */
UENUM(BlueprintType)
enum class EGSTN_DrillLaunchDirection : uint8
{
	/** Away from the target, along the surface normal at the latch point */
	SurfaceNormal,

	/** Against the entity's gravity */
	GravityUp,
};

/** Air spin: the small hop and the low gravity while spinning in the air */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_AirSpinTuning
{
	GENERATED_BODY()

	/** Speed of the hop added when the air spin starts (momentum kept) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|AirSpin", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float HopSpeed = 450.f;

	/** Gravity multiplier during the air spin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|AirSpin", meta = (ClampMin = "0", ClampMax = "2"))
	float SpinGravityScale = 0.35f;

	/** Fall speed cap multiplier during the air spin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|AirSpin", meta = (ClampMin = "0", ClampMax = "2"))
	float SpinMaxFallSpeedScale = 0.5f;

	/** Frames the low gravity outlives the spin timeline; 0 = only during the spin (14 SD.3) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|AirSpin", meta = (ClampMin = "0"))
	int32 LowGravityExtraFrames = 0;

	/** Air spins allowed per airtime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|AirSpin", meta = (ClampMin = "0", ClampMax = "3"))
	int32 UsesPerAirtime = 1;

	/** Events that give the air spins back (EGSTN_AirSpinRefresh flags) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|AirSpin", meta = (Bitmask, BitmaskEnum = "/Script/GSTNSpinDrillCore.EGSTN_AirSpinRefresh"))
	int32 RefreshOn = static_cast<int32>(EGSTN_AirSpinRefresh::Land | EGSTN_AirSpinRefresh::Wall | EGSTN_AirSpinRefresh::Trampoline | EGSTN_AirSpinRefresh::Launch);
};

/** Opposite-spin cancel (spin-drill.md 7.3) */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_SpinCancelTuning
{
	GENERATED_BODY()

	/** First frame of the spin from which the opposite rotation cancels it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Cancel", meta = (ClampMin = "0"))
	int32 CancelFromFrame = 6;

	/** Recovery frames of the cancel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Cancel", meta = (ClampMin = "0"))
	int32 CancelRecoveryFrames = 8;
};

/** Dive: the plunge before a latch (spin-drill.md 7.4, 13b: lateral steering) */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_DiveTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Dive")
	EGSTN_DrillDiveDirection DiveDirection = EGSTN_DrillDiveDirection::Gravity;

	/** Speed along the dive direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Dive", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float PlungeSpeed = 1800.f;

	/** Lateral steering, on the plane normal to the dive direction, as a fraction of PlungeSpeed at full stick (13b: 15 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Dive", meta = (ClampMin = "0", ClampMax = "1"))
	float SteerFraction = 0.15f;

	/** Frames of dive before the tip may latch (no latch on the frame the dive starts) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Dive", meta = (ClampMin = "0"))
	int32 MinDiveFramesBeforeLatch = 2;

	/** Landing lag when the dive reaches plain ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Dive", meta = (ClampMin = "0"))
	int32 GroundLagFrames = 18;

	/** Speed of the bounce off a hittable but not drillable target, against the dive direction (spin-drill.md 7.6) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Dive", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float BounceSpeed = 700.f;
};

/** Bore: latched on a target (spin-drill.md 7.4 with 13b: no MinBoreFramesForLaunch, early release = hop out, jump boost) */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_BoreTuning
{
	GENERATED_BODY()

	/** Frames of the seat, no damage, wobble plays; scaled by the target's SeatFramesScale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore", meta = (ClampMin = "0"))
	int32 SeatFrames = 12;

	/** Frames between two hits on the latched target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore", meta = (ClampMin = "1"))
	int32 RehitIntervalFrames = 6;

	/** Longest bore, counted from the latch; the target may override it (FGSTN_DrillTargetResponse::MaxBoreFramesOverride) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore", meta = (ClampMin = "1"))
	int32 MaxBoreFrames = 90;

	/** Speed of the small hop out on an early release, or at the maximum time without a jump input (13b) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float DetachPopSpeed = 400.f;

	/** Frames before the exit frame in which a jump press or hold turns the exit into the high jump (13b) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore", meta = (ClampMin = "0"))
	int32 JumpBoostWindowFrames = 8;

	/** How deep the tip sits in the target */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LatchDepthCm = 10.f;

	/** Keep plunging when the target breaks instead of the final jump (14 SD.7) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Bore")
	bool bContinueThroughDestroyed = false;
};

/** Launch: the high jump out of a target */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_DrillLaunchTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Launch", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LaunchSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Launch")
	EGSTN_DrillLaunchDirection LaunchDirection = EGSTN_DrillLaunchDirection::SurfaceNormal;

	/** The launch gives the air spins back (it raises EGSTN_AirSpinRefresh::Launch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Launch")
	bool bRefreshAirSpin = true;
};

/** Wobble of the drill while it seats (presentation, spin-drill.md 9) */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_DrillWobbleTuning
{
	GENERATED_BODY()

	/** Amplitude over the seat, both axes normalised to 0..1; sampled once into an array (UGSTN_DrillMath::SampleWobbleCurve). None = linear decay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Wobble")
	TSoftObjectPtr<UCurveFloat> WobbleCurve;

	/** Tilt of the drill at full amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Wobble", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float MaxWobbleDegrees = 12.f;

	/** Turns per second of the tilt around the drill axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Wobble", meta = (ClampMin = "0", ForceUnits = "Hz"))
	float PrecessionHz = 3.f;

	/** Visual spin of the mesh, turns per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Wobble", meta = (ClampMin = "0", ForceUnits = "Hz"))
	float VisualSpinRps = 6.f;
};
