// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_SpinDrillTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_DrillMath.generated.h"

class UCurveFloat;

/** Tip position in the target's root space, 1 cm per unit (spin-drill.md 7.1 AnchorQ: 3 x 16 bits, about +-327 m) */
struct FGSTN_DrillAnchorQ
{
	int16 X = 0;
	int16 Y = 0;
	int16 Z = 0;

	bool operator==(const FGSTN_DrillAnchorQ& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
};

/** Drill axis in the target's space, octahedral (spin-drill.md 7.1 AxisQ: 2 x 16 bits); (0, 0) is +Z */
struct FGSTN_DrillAxisQ
{
	int16 X = 0;
	int16 Y = 0;

	bool operator==(const FGSTN_DrillAxisQ& Other) const { return X == Other.X && Y == Other.Y; }
};

/** What ends a bore this frame (spin-drill.md 7.4 order, with the owner answers of 13b) */
UENUM(BlueprintType)
enum class EGSTN_DrillEndResult : uint8
{
	/** Keep boring */
	Continue,

	/** The driller was staggered or knocked out: detach, the hitstun wins, no launch */
	DetachHitstun,

	/** The small hop out (early release, or the maximum time without a jump input) */
	DetachPop,

	/** The high jump out (jump boost at the exit, or the target broke) */
	Launch,

	/** The target broke and the tuning keeps plunging through it (14 SD.7 option) */
	KeepPlunging,
};

/** Inputs of the end-condition resolver, one frame of a latched driller */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_DrillEndInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bDrillerInHitstun = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bDrillerKnockedOut = false;

	/** The target left the snapshot (destroyed, despawned) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bTargetGone = false;

	/** The target is still there but no longer drillable (its part broke, a refusal became active) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bTargetNotDrillable = false;

	/** Copied from FGSTN_BoreTuning::bContinueThroughDestroyed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bContinueThroughDestroyed = false;

	/** Crouch (the drill's trigger) is still held, read from the input history */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bCrouchHeld = true;

	/** Frames since the latch frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End", meta = (ClampMin = "0"))
	int32 FramesBored = 0;

	/** Effective maximum (UGSTN_DrillMath::ResolveMaxBoreFrames) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End", meta = (ClampMin = "1"))
	int32 MaxBoreFrames = 1;

	/** A jump press or hold inside the boost window (UGSTN_DrillMath::IsJumpBoostInWindow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|End")
	bool bJumpBoost = false;
};

/** Wobble of a seating drill at one frame (presentation) */
USTRUCT(BlueprintType)
struct GSTNSPINDRILLCORE_API FGSTN_DrillWobble
{
	GENERATED_BODY()

	/** Tilt of the drill away from its axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Wobble", meta = (ForceUnits = "deg"))
	float AmplitudeDegrees = 0.f;

	/** Direction of the tilt around the axis, 0..360 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinDrill|Wobble", meta = (ForceUnits = "deg"))
	float PhaseDegrees = 0.f;
};

/**
 * Pure, deterministic math of the spin and drill (spin-drill.md 7, 9, 13b). No world, no component, no UObject read on the simulation
 * path: every input is a value. The one exception, SampleWobbleCurve, reads a curve asset once, outside the simulation, into an
 * array that ComputeWobble then reads.
 *
 * Determinism: integer frame arithmetic for every decision (end conditions, refresh, windows); floating point only for vectors and
 * the presentation wobble, with the same operations on every machine. Quantised forms (anchor, axis) are what the drill fragment
 * stores, so every machine works from the same bits after a rollback.
 */
UCLASS()
class GSTNSPINDRILLCORE_API UGSTN_DrillMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Quantisation (C++ only: int16 is not a Blueprint type) ----

	/**
	 * Anchor in the target's root space to 1 cm integers, rounded to nearest. Error at most 0.5 cm per axis inside the range.
	 * @return false when a component was outside +-327.67 m and was clamped.
	 */
	static bool QuantizeAnchor(const FVector& LocalCm, FGSTN_DrillAnchorQ& OutAnchor);
	static FVector DequantizeAnchor(const FGSTN_DrillAnchorQ& Anchor);

	/** Unit axis to octahedral 2 x int16 (any length accepted, normalised first; a zero vector encodes +Z). Error below 0.01 degree */
	static FGSTN_DrillAxisQ EncodeAxis(const FVector& Axis);

	/** Always a unit vector */
	static FVector DecodeAxis(const FGSTN_DrillAxisQ& Axis);

	// ---- Frames ----

	/** Seat length on a target: SeatFrames x the target's scale, rounded, never negative */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static int32 ResolveSeatFrames(int32 SeatFrames, float SeatFramesScale);

	/** Longest bore on a target: its override when positive, the tuning's otherwise; at least 1 */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static int32 ResolveMaxBoreFrames(int32 TuningMaxBoreFrames, int32 TargetOverride);

	/**
	 * Jump boost (13b): jump held now, or pressed at most WindowFrames before Now. LastJumpPressFrame comes from the input history
	 * (INDEX_NONE when there is none), so the window is buffered and rollback-safe.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static bool IsJumpBoostInWindow(int32 Now, int32 LastJumpPressFrame, bool bJumpHeld, int32 WindowFrames);

	/**
	 * End of a bore, first match wins: driller in hitstun or KO -> DetachHitstun; target gone or not drillable -> Launch (KeepPlunging
	 * with bContinueThroughDestroyed); crouch released -> Launch with a jump boost, DetachPop without; maximum reached -> the same;
	 * otherwise Continue.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static EGSTN_DrillEndResult ResolveEnd(const FGSTN_DrillEndInputs& Inputs);

	// ---- Air spin ----

	/** True when one of this frame's events (EGSTN_AirSpinRefresh flags) is in the tuning's RefreshOn */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static bool ShouldRefreshAirSpin(int32 RefreshOnMask, int32 EventMask);

	/** Air spins used after this frame's events: 0 when refreshed, unchanged otherwise */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static int32 AirSpinsUsedAfterEvents(int32 AirSpinsUsed, int32 RefreshOnMask, int32 EventMask);

	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static bool CanAirSpin(int32 AirSpinsUsed, int32 UsesPerAirtime);

	/** Refresh events raised by the end of a bore: Launch for a launch when the launch tuning refreshes, nothing else */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static int32 RefreshEventsForEnd(EGSTN_DrillEndResult Result, bool bLaunchRefreshesAirSpin);

	// ---- Vectors ----

	/**
	 * Velocity of a launch or a hop out. SurfaceNormal: along the normal at the latch point (away from the target); GravityUp: against
	 * Gravity, any direction. A zero normal falls back to gravity-up and a zero gravity to the normal; both zero gives zero.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static FVector ComputeLaunchVelocity(EGSTN_DrillLaunchDirection Direction, const FVector& SurfaceNormal, const FVector& Gravity, float Speed);

	/**
	 * Dive velocity with the lateral steering of 13b: PlungeSpeed along DiveDirection plus the steer input's part on the plane normal to
	 * it (capped to length 1) times PlungeSpeed x SteerFraction. Steering never changes the speed along the dive.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static FVector ComputeDiveVelocity(const FVector& DiveDirection, const FVector& SteerInput, float PlungeSpeed, float SteerFraction);

	// ---- Wobble (presentation) ----

	/**
	 * Wobble at FramesSinceLatch. Amplitude = MaxWobbleDegrees x the normalised amplitude at FramesSinceLatch / SeatFrames, read
	 * linearly from AmplitudeSamples (evenly spaced over the seat; empty = linear decay from 1 to 0), and exactly 0 from SeatFrames
	 * on whatever the samples say. Phase turns at PrecessionHz on the fixed tick.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|SpinDrill")
	static FGSTN_DrillWobble ComputeWobble(int32 FramesSinceLatch, int32 SeatFrames, float MaxWobbleDegrees, float PrecessionHz,
		float FixedTickHz, const TArray<float>& AmplitudeSamples);

	/** Samples a curve whose time and value run 0..1 at Count even times (Count >= 2). A null curve gives an empty array. Not for the simulation */
	UFUNCTION(BlueprintCallable, Category = "GSTN|SpinDrill")
	static void SampleWobbleCurve(const UCurveFloat* Curve, int32 Count, TArray<float>& OutSamples);
};
