// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_SpinDrillTracks.h"
#include "Timeline/GSTN_TimelineTypes.h"

/**
 * What a default spin timeline is made of (spin-drill.md 2: 3D World on the ground, Sunshine in the air). The abilities of the default kit
 * build their inline timelines from these values; a kit that wants another spin authors a timeline asset instead, and nothing in the
 * framework depends on the numbers.
 */
struct FGSTN_SpinTimelineParams
{
	int32 TotalFrames = 30;
	int32 FirstActionableFrame = 18;

	/** The hitbox ring around the body, [RingStartFrame, RingEndFrame) */
	int32 RingStartFrame = 2;
	int32 RingEndFrame = 16;
	float RingRadius = 110.f;

	/** Height of the ring's centre above the root */
	float RingHeight = 40.f;

	float Damage = 3.f;
	float BaseKnockback = 300.f;
	float KnockbackAngleDeg = 30.f;

	/** The part group the ring hangs on: the whole body, spelled as the group that holds every part (an empty selector is refused) */
	FName RingGroup = TEXT("Body");

	/**
	 * First frame of the cancel window into the opposite spin command (D5). Keep it equal to the kit tuning's
	 * FGSTN_SpinCancelTuning::CancelFromFrame: the window is the gate, the tuning value is what the phase machine checks. Negative = no window
	 */
	int32 CancelFromFrame = 6;

	/** First frame of the cancel window into the drill command (D8, spec 2: "entry from spin frame 3"). Negative = no window */
	int32 DiveFromFrame = 3;
};

/** What the default drill dive timeline is made of (D8) */
struct FGSTN_DiveTimelineParams
{
	/** The longest dive; the dive ends earlier on the ground (lag), on a bounce, or when crouch is released */
	int32 MaxFrames = 120;

	/** The tip: a sphere below the feet, along the character's down (the dive goes along gravity and the character stands along it) */
	float TipRadius = 45.f;
	float TipDepth = 20.f;

	float Damage = 4.f;
	float BaseKnockback = 200.f;
	float KnockbackAngleDeg = 60.f;

	/** The tip is part of the whole body group, as the spin's ring */
	FName TipGroup = TEXT("Body");
};

/** What the default drill bore timeline is made of (D9 part 2): seat frame 0, bore frame 1 (held), then the recovery */
struct FGSTN_BoreTimelineParams
{
	/** Frames played after the latch ends (launch, pop) before the timeline is over */
	int32 RecoveryFrames = 8;

	/** Keep equal to the kit tuning's FGSTN_BoreTuning::RehitIntervalFrames: the hitbox carries it, the tuning documents it */
	int32 RehitIntervalFrames = 6;

	/** The tip, as the dive's */
	float TipRadius = 45.f;
	float TipDepth = 20.f;
	float Damage = 2.f;
	float BaseKnockback = 0.f;
	float KnockbackAngleDeg = 90.f;
	FName TipGroup = TEXT("Body");
};

/** What the default opposite-spin cancel timeline is made of (D5) */
struct FGSTN_SpinCancelTimelineParams
{
	/** The recovery: no hitbox, actionable at the end (FGSTN_SpinCancelTuning::CancelRecoveryFrames) */
	int32 RecoveryFrames = 8;
};

/** Builders of the default spin timelines, as plain data */
struct GSTNSPINDRILLCORE_API FGSTN_SpinTimelines
{
	/**
	 * A spin: the spin track over the whole length, the hitbox ring, a cancel window into the jump over the whole length (3D World: the
	 * spin chains into a jump), one into the opposite spin command from CancelFromFrame (D5) and one into the drill command from
	 * DiveFromFrame (D8). No movement lock: locomotion continues.
	 */
	static FGSTN_CombatTimelineData Make(const FGSTN_SpinTimelineParams& Params, EGSTN_SpinSense Sense, bool bAirSpin);

	/** The drill dive: the dive track over the whole length and the tip hitbox (Combat.HitType.Drill, swept between frames: the dive is fast) */
	static FGSTN_CombatTimelineData MakeDive(const FGSTN_DiveTimelineParams& Params);

	/** The drill bore: the bore track (seat frame 0, held bore frame 1, crouch) and the tip hitbox on the bore frame with its re-hit interval */
	static FGSTN_CombatTimelineData MakeBore(const FGSTN_BoreTimelineParams& Params);

	/** The opposite-spin cancel: the cancel track over the recovery, a jump cancel window, nothing else. Sense = the cancelling rotation */
	static FGSTN_CombatTimelineData MakeCancel(const FGSTN_SpinCancelTimelineParams& Params, EGSTN_SpinSense Sense);

	/** The spin command of a sense */
	static FGameplayTag SpinCommand(EGSTN_SpinSense Sense);

	static EGSTN_SpinSense Opposite(EGSTN_SpinSense Sense)
	{
		return Sense == EGSTN_SpinSense::Clockwise ? EGSTN_SpinSense::CounterClockwise : EGSTN_SpinSense::Clockwise;
	}

	static FGSTN_SpinTimelineParams GroundParams();

	/** The air spin: the same ring, a little longer so the low gravity reads as a hover */
	static FGSTN_SpinTimelineParams AirParams();
};
