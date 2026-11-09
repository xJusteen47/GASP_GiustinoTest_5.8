// Projectile, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitShape.h"

#include "GSTN_BallisticMode.generated.h"

/**
 * What a projectile is born with (entita-dinamiche review §2, §3): the only thing that crosses the wire. Every machine computes the
 * projectile's position from these values and the frame number, so nothing is replicated per tick and a rewound frame is just another
 * PositionAt call. Units are cm, cm/s and fixed-tick frames.
 */
USTRUCT(BlueprintType)
struct GSTNPROJECTILECORE_API FGSTN_ProjectileParams
{
	GENERATED_BODY()

	/** World position at the spawn frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Motion", meta = (ForceUnits = "cm"))
	FVector Origin = FVector::ZeroVector;

	/** Normalized on use; a zero direction fires along +X */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Motion")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Motion", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float Speed = 1500.f;

	/**
	 * Constant acceleration taken at the spawn point (combat-m10 §9: gravity is a vector; a gravity change would start a new segment,
	 * which this slice does not do). Zero flies straight
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Motion")
	FVector GravityAccel = FVector::ZeroVector;

	/** Frames after the spawn frame the projectile lives; it is removed after its last frame without a hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Motion", meta = (ClampMin = "1"))
	int32 LifetimeFrames = 120;

	/** Combat pipeline id of whoever fired it (the AttackerId of its hits); INDEX_NONE for an owner-less projectile */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Owner")
	int32 OwnerEntityId = INDEX_NONE;

	/** The owner's team when it fired (credit and relationship survive a team change, combat-m10 §10); 255 = no team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Owner")
	uint8 OwnerTeamId = 255;

	/** The hitbox in projectile space (X along the velocity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Hit")
	FGSTN_HitShape Shape;

	/** What a hit carries into the victim's pipeline, exactly as a melee hitbox's data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Hit")
	FGSTN_HitData Hit;

	/** Distinct victims it may hit before it is consumed (1 = gone on the first hit; more pierces) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Hit", meta = (ClampMin = "1"))
	uint8 MaxHits = 1;
};

/**
 * One ballistic segment: the parameters plus the frame they start at and the step length. The position at StartFrame is Start; the
 * first frame that moves (and can hit) is StartFrame + 1.
 */
USTRUCT(BlueprintType)
struct GSTNPROJECTILECORE_API FGSTN_BallisticSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	int32 StartFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector GravityAccel = FVector::ZeroVector;

	/** Seconds per fixed-tick frame (0.016 at 62.5 Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	double StepSeconds = 0.016;
};

/**
 * The "ballistic" movement mode (entita-dinamiche review §8): the rules a launched body follows, as a closed form of the frame number.
 * A mode in the sense of "how it moves", separate from the transport (here: one spawn event). No shared mode interface yet on purpose:
 * it will be extracted from the first real case, when the fighters' movement is redone as its own mode.
 *
 * Determinism: double math on the segment and an integer frame, no accumulated state, so the same frame computed on the host, on a
 * client or in a rewind gives the same bits for the same inputs.
 */
struct GSTNPROJECTILECORE_API FGSTN_BallisticMode
{
	/** The segment that Params describe, starting at StartFrame */
	static FGSTN_BallisticSegment MakeSegment(const FGSTN_ProjectileParams& Params, int32 StartFrame, double StepSeconds);

	/** Start + V t + G t^2 / 2 with t = (Frame - StartFrame) * StepSeconds; frames before the start give the start */
	static FVector PositionAt(const FGSTN_BallisticSegment& Segment, int32 Frame);

	static FVector VelocityAt(const FGSTN_BallisticSegment& Segment, int32 Frame);

	/** The hitbox's carrier at Frame: the position, X along the velocity (along the launch velocity while that is zero) */
	static FTransform CarrierAt(const FGSTN_BallisticSegment& Segment, int32 Frame);

	/**
	 * Whether Shape, carried from Frame - 1 to Frame, touches Target placed at TargetWorld: the swept test of a melee hitbox with
	 * bSweepBetweenFrames, so a fast projectile does not jump over a thin hurtbox
	 */
	static bool SweepStep(const FGSTN_BallisticSegment& Segment, const FGSTN_HitShape& Shape, int32 Frame, const FGSTN_HitShape& Target, const FTransform& TargetWorld);

	/**
	 * The arc variant: the launch direction that brings a body fired from Origin at Speed under GravityAccel through Target (the
	 * classic two-solution ballistic aim, solved in the plane of gravity, so a gravity that is not -Z works too). bHighArc picks the
	 * lob instead of the flat shot. Zero gravity gives the straight line. False (OutDirection untouched) when Speed cannot reach Target.
	 * Pure double math on the inputs: the host and a client aiming the same shot get the same bits
	 */
	static bool SolveLaunchDirection(const FVector& Origin, const FVector& Target, double Speed, const FVector& GravityAccel, bool bHighArc, FVector& OutDirection);

	/** The first frame in (StartFrame, LastFrame] whose SweepStep touches Target, INDEX_NONE when none does. For tests and tools */
	static int32 FindFirstContactFrame(const FGSTN_BallisticSegment& Segment, const FGSTN_HitShape& Shape, const FGSTN_HitShape& Target, const FTransform& TargetWorld, int32 LastFrame);
};
