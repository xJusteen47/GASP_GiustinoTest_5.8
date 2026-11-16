// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_BalloonKnockback.generated.h"

/**
 * "Balloon" knockback (owner 2026-11-16, after Smash Ultimate): a strong launch flies the SAME path as without it, but faster at the start and
 * normal at the end, so the victim spends less time in the air and in hitstun. It is a time warp of the knockback flight, not a different
 * trajectory: on game frame n the knockback move advances the flight by M(n) frames of "flight time" instead of one.
 *
 * Model (public frame data research on Ultimate, ssbwiki "Hitstun"): with H = the hitstun the knockback model computed (flight frames),
 *  - the first frame's multiplier M0 goes from 1 at H = MinHitstunFrames to MaxSpeedMultiplier at H = FullHitstunFrames (linear, clamped);
 *  - each frame the warped clock T advances by M(n); with r = (H - T) / H the flight left, M(n) = max(1, M0 x (r - EndFraction) / (1 - EndFraction)),
 *    so the speed-up fades out linearly and is gone once EndFraction of the flight is left (Ultimate: 0.3);
 *  - the hitstun in game frames is the number of frames the warped clock needs to reach H: the lock ends at the same point of the path.
 * Ultimate also exempts set knockback and applies it to tumble only: both are switches here.
 *
 * Off by default. Deterministic: every value is a pure function of (settings, H, frame index), evaluated with the same loop on every machine.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_BalloonKnockbackSettings
{
	GENERATED_BODY()

	/** Off = the classic flight (one flight frame per game frame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon")
	bool bEnabled = false;

	/** At or below this hitstun there is no speed-up (Ultimate ~30 frames) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon", meta = (ClampMin = "1", ForceUnits = "Frames", EditCondition = "bEnabled"))
	int32 MinHitstunFrames = 30;

	/** At or above this hitstun the first frame runs at MaxSpeedMultiplier (Ultimate ~80 frames) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon", meta = (ClampMin = "2", ForceUnits = "Frames", EditCondition = "bEnabled"))
	int32 FullHitstunFrames = 80;

	/** The first frame's multiplier at FullHitstunFrames (Ultimate 6) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon", meta = (ClampMin = "1.0", ClampMax = "10.0", EditCondition = "bEnabled"))
	float MaxSpeedMultiplier = 6.f;

	/** The fraction of the flight left when the speed-up has faded to nothing (Ultimate 0.3) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon", meta = (ClampMin = "0.0", ClampMax = "0.95", EditCondition = "bEnabled"))
	float EndFraction = 0.3f;

	/** Only launches that tumble balloon (as in Ultimate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon", meta = (EditCondition = "bEnabled"))
	bool bTumbleOnly = true;

	/** Hits with FixedKnockback (and launches asked by an applier) balloon too. Off as in Ultimate: set knockback keeps the classic flight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balloon", meta = (EditCondition = "bEnabled"))
	bool bApplyToFixedKnockback = false;
};

/** The math of FGSTN_BalloonKnockbackSettings. Pure and deterministic */
struct GSTNCOMBATCORE_API FGSTN_BalloonMath
{
	/** The largest flight the sync state carries (8 bits); a longer hitstun is clamped for the warp, which only matters past it */
	static constexpr int32 MaxFlightFrames = 255;

	/** M0 for a flight of FlightFrames, from the settings; 1 = no speed-up */
	static double InitialMultiplier(int32 MinHitstunFrames, int32 FullHitstunFrames, double MaxSpeedMultiplier, int32 FlightFrames);
	static double InitialMultiplier(const FGSTN_BalloonKnockbackSettings& Settings, int32 FlightFrames);

	/** M at warped clock Consumed (flight frames already flown) */
	static double Multiplier(double M0, double EndFraction, int32 FlightFrames, double Consumed);

	/**
	 * Walks the warp to game frame FrameIndex (0 = the launch frame): OutConsumed = flight frames flown before it, OutMultiplier = M on it.
	 * FrameIndex past the flight returns M = 1
	 */
	static void Evaluate(double M0, double EndFraction, int32 FlightFrames, int32 FrameIndex, double& OutConsumed, double& OutMultiplier);

	/** The game frames the warped clock needs to fly FlightFrames (<= FlightFrames) */
	static int32 GameFrames(double M0, double EndFraction, int32 FlightFrames);

	/** The first game frame on which M is back to 1 (0 when there is no speed-up) */
	static int32 WarpEndFrame(double M0, double EndFraction, int32 FlightFrames);
};
