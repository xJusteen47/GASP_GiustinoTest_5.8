// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_HitBounce.generated.h"

/**
 * Hit Bounce (owner 2026-11-23, the Overlord's Cr.L Aerial stomp; scope: any move that carries the track): a hit the move lands inside
 * [StartFrame, EndFrame) makes its user bounce off the target. After the attacker's hitstop the move ends (no hitbox during the bounce), the
 * user is launched up ApexHeightCm and back BackDistanceCm in RiseFrames, and is locked for ActionableFrames from the launch: then it is free
 * (an aerial, an air jump if it has one left, an air dodge). Any hit counts (owner 2B), never a whiff, a blocked or parried hit, a clank or a
 * ricochet. At most MaxConsecutiveBounces in a row (owner 3A, set by hand): the count resets on landing and on being hit. At the limit the
 * move plays as a normal move (its hold, its landing). A track of no known kind: the player ignores it, the combat model's hit bounce
 * extension (GSTNCombatNP) reads it by struct. Every number here has a gstn.Combat.Debug.HitBounce* override (FGSTN_HitBounceOverrides).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hit Bounce"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_HitBounce : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** Bounces in a row before the move stops bouncing (0 = never bounces). Reset on landing and on being hit. Owner 2026-11-23: 2 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce", meta = (ClampMin = "0", ClampMax = "15"))
	int32 MaxConsecutiveBounces = 2;

	/** Height of the bounce's apex above the launch point, along the user's up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApexHeightCm = 150.f;

	/** Frames (62.5 per second) from the launch to the apex; the bounce's own gravity is chosen so both numbers hold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce", meta = (ClampMin = "1", ClampMax = "120", ForceUnits = "Frames"))
	int32 RiseFrames = 18;

	/** Distance travelled against the facing during the rise (negative = forward) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce", meta = (ForceUnits = "cm"))
	float BackDistanceCm = 40.f;

	/** Frames from the launch during which the user cannot act; free from this frame on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce", meta = (ClampMin = "1", ClampMax = "120", ForceUnits = "Frames"))
	int32 ActionableFrames = 24;

	/** A blocked (shielded) hit bounces too. Owner 2026-11-23: no */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce")
	bool bBounceOnBlock = false;

	/**
	 * Optional: the timeline ability (a UGSTN_GA_BaseCombat child: the backflip clip) started Forced on the launch frame in place of simply ending
	 * the move. Presentation and frame data only: the launch and the lock above stay the model's. It must be granted by the kit (its
	 * TriggeredAbilities) and its own first actionable frame should not be later than ActionableFrames; when it is not registered the move just ends
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Bounce", meta = (AllowedClasses = "/Script/GSTNCombatNP.GSTN_GA_BaseCombat"))
	TSoftClassPtr<UObject> BounceMove;
};

/** Live overrides of a track's numbers (the gstn.Combat.Debug.HitBounce* console variables). Below 0 = the track's value */
struct FGSTN_HitBounceOverrides
{
	float MaxConsecutiveBounces = -1.f;
	float ApexHeightCm = -1.f;
	float RiseFrames = -1.f;
	/** Below 0 = the track's: the override only sets a backward distance (a forward one is data only) */
	float BackDistanceCm = -1.f;
	float ActionableFrames = -1.f;
	/** Turns the bounce off everywhere: every move plays as without the track */
	bool bDisabled = false;
};

/** The numbers a bounce uses, the track's with the overrides applied and clamped */
struct FGSTN_HitBounceTuning
{
	int32 MaxConsecutiveBounces = 2;
	float ApexHeightCm = 150.f;
	int32 RiseFrames = 18;
	float BackDistanceCm = 40.f;
	int32 ActionableFrames = 24;
	bool bBounceOnBlock = false;
	bool bEnabled = true;
};

/** The launch of one bounce: what the move effect queued on the user's Mover carries */
struct FGSTN_HitBounceLaunch
{
	/** World velocity of the launch frame, cm/s */
	FVector Velocity = FVector::ZeroVector;
	/** Multiplier of the movement mode's gravity while the rise lasts (the move adds the difference) */
	float GravityScale = 1.f;
	/** Lifetime of the move: the rise */
	int32 Frames = 0;
};

/** The rules of the Hit Bounce track, pure functions for the combat model's extension and the tests */
struct GSTNCOMBATCORE_API FGSTN_HitBounceRules
{
	/** The first Hit Bounce track of Data, null when it has none (OutIndex its index) */
	static const FGSTN_TimelineTrack_HitBounce* FindTrack(const FGSTN_CombatTimelineData& Data, int32* OutIndex = nullptr);

	/** Track's numbers with the overrides that are set (>= 0), clamped to the track's ranges */
	static FGSTN_HitBounceTuning Resolve(const FGSTN_TimelineTrack_HitBounce& Track, const FGSTN_HitBounceOverrides& Overrides);

	/** Another bounce is allowed after Count bounces in a row */
	static bool CanBounce(int32 Count, const FGSTN_HitBounceTuning& Tuning);

	/** A hit that bounces: not a ricochet, not parried, not a clank, blocked only when the tuning allows it */
	static bool IsBouncingHit(bool bRicochet, bool bBlocked, bool bParried, bool bClank, const FGSTN_HitBounceTuning& Tuning);

	/**
	 * The launch: up speed 2H/T and a gravity of 2H/T^2 (T = RiseFrames * StepSeconds), so the apex is ApexHeightCm after RiseFrames; the back
	 * speed BackDistanceCm/T against Facing on the plane of Up. ModeGravity is the size of the movement mode's gravity (cm/s^2) the scale is taken
	 * against; at or near zero the scale stays 1 (the rise then does not slow). Up and Facing need not be normalised; any gravity direction works
	 */
	static FGSTN_HitBounceLaunch ComputeLaunch(const FGSTN_HitBounceTuning& Tuning, const FVector& Up, const FVector& Facing, double ModeGravity, double StepSeconds);
};
