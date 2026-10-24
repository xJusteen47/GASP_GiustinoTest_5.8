// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Movement/GSTN_DashTypes.h"
#include "Timeline/GSTN_TimelineTypes.h"

/**
 * What a movement timeline (dash, ground dodge, air dodge) is made of. The timelines of the default kit are built from these
 * values by FGSTN_MoveTimelines, and a kit that wants another dodge builds its own or authors a timeline asset: nothing in the
 * framework depends on the numbers.
 */
struct FGSTN_MoveTimelineParams
{
	int32 TotalFrames = 12;
	int32 FirstActionableFrame = 12;

	/** The movement track covers [0, TotalFrames). The ability ends the timeline on landing when set (it reads the track) */
	bool bCancelOnLand = false;
	int32 LandingLagFrames = 0;
	EGSTN_MoveMixMode MixMode = EGSTN_MoveMixMode::Override;
	EGSTN_MomentumMode MomentumMode = EGSTN_MomentumMode::Discard;

	/** Frames from take-off the hurtboxes of HurtboxGroup are invincible; 0 = no hurtbox track */
	int32 InvincibleFrames = 0;
	FName HurtboxGroup = TEXT("Feet");
	EGSTN_HurtboxModifierKind HurtboxKind = EGSTN_HurtboxModifierKind::Invincible;
};

/** Builders of the default movement timelines, as plain data */
struct GSTNCOMBATCORE_API FGSTN_MoveTimelines
{
	/** Fraction of the airborne frames the feet of a ground dodge are invincible for, counted from take-off (User answers (c) 2) */
	static constexpr float DefaultInvincibleFraction = 0.4f;

	/** Nominal gravity (cm/s^2) the default arcs are tuned against; only used to turn an arc into frames when building data */
	static constexpr float NominalGravity = 980.f;

	/** The fixed tick rate (Hz) the frame counts of the default timelines assume */
	static constexpr float DefaultTickRate = 60.f;

	/** The frames of the invincibility window: Fraction of AirFrames, rounded, at least 1 while there is any air time */
	static int32 InvincibleFramesFor(int32 AirFrames, float Fraction);

	/** A timeline with one movement track over its whole length and, if InvincibleFrames > 0, a hurtbox modifier from frame 0 */
	static FGSTN_CombatTimelineData Make(const FGSTN_MoveTimelineParams& Params);

	/** The default dash: a short ground burst (12 frames), no invincibility */
	static FGSTN_MoveTimelineParams DashParams();

	/** The default ground dodge: take-off to landing in one low arc, feet invincible for the first 40 % of the airborne frames, lands standing */
	static FGSTN_MoveTimelineParams GroundDodgeParams(const FGSTN_DashMoveData& Move, float TickRate);

	/** The default air dodge (dive roll): ends on landing with a short landing lag, feet invincible for the first part of its dive */
	static FGSTN_MoveTimelineParams AirDodgeParams();

	/** The default impulses */
	static FGSTN_DashMoveData DashMove();
	static FGSTN_DashMoveData GroundDodgeMove();
	static FGSTN_DashMoveData AirDodgeMove();
};
