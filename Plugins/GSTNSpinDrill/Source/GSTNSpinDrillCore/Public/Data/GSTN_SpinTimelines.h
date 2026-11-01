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
};

/** Builders of the default spin timelines, as plain data */
struct GSTNSPINDRILLCORE_API FGSTN_SpinTimelines
{
	/**
	 * A spin: the spin track over the whole length, the hitbox ring, and a cancel window into the jump over the whole length (3D World:
	 * the spin chains into a jump). No movement lock: locomotion continues. The opposite-spin cancel (D5) and the drill (D8) add their
	 * own windows.
	 */
	static FGSTN_CombatTimelineData Make(const FGSTN_SpinTimelineParams& Params, EGSTN_SpinSense Sense, bool bAirSpin);

	static FGSTN_SpinTimelineParams GroundParams();

	/** The air spin: the same ring, a little longer so the low gravity reads as a hover */
	static FGSTN_SpinTimelineParams AirParams();
};
