// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MomentumSlide.generated.h"

/**
 * What a ground move started while running does with the running speed (owner 2026-11-13: "many ground attacks slide on their feet
 * along the momentum while running; done the other way the behaviour may differ; both per move").
 */
UENUM(BlueprintType)
enum class EGSTN_MomentumSlideMode : uint8
{
	/** The running speed is dropped on the first frame: the move stands where it started (the behaviour of every move before the slide existed) */
	Stop,
	/** The character keeps sliding on its feet along the running direction, slowed by the friction of the response, while the move plays */
	Slide
};

/** Where the friction of a slide comes from: two plausible models, so both are offered */
UENUM(BlueprintType)
enum class EGSTN_MomentumSlideFriction : uint8
{
	/** A constant deceleration of the move's own (Deceleration, cm/s^2): the speed falls in a straight line, like a fighting game's traction */
	Fixed,
	/**
	 * The braking of the character's ground movement (Mover's shared legacy settings: Deceleration plus BrakingFriction * BrakingFrictionFactor
	 * per unit of speed), scaled by FrictionScale. A heavier or more slippery kit then slides differently with the same move data
	 */
	Character
};

/** One way a move answers the running speed: what it does when started along the momentum, and what it does when started against it */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_MomentumSlideResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide")
	EGSTN_MomentumSlideMode Mode = EGSTN_MomentumSlideMode::Slide;

	/** Fraction of the running speed the slide starts with (1 = all of it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (EditCondition = "Mode == EGSTN_MomentumSlideMode::Slide", ClampMin = "0.0", ClampMax = "2.0"))
	float SpeedScale = 1.f;

	/** Cap of the starting speed after SpeedScale, 0 = no cap */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (EditCondition = "Mode == EGSTN_MomentumSlideMode::Slide", ClampMin = "0.0", ForceUnits = "cm/s"))
	float MaxStartSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (EditCondition = "Mode == EGSTN_MomentumSlideMode::Slide"))
	EGSTN_MomentumSlideFriction Friction = EGSTN_MomentumSlideFriction::Fixed;

	/** The constant deceleration of the Fixed friction. 1500 takes a 600 cm/s run to rest in 0.4 s, about 120 cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (EditCondition = "Mode == EGSTN_MomentumSlideMode::Slide && Friction == EGSTN_MomentumSlideFriction::Fixed", ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float Deceleration = 1500.f;

	/** Multiplies the character's braking with the Character friction (0.25 = four times as slippery) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (EditCondition = "Mode == EGSTN_MomentumSlideMode::Slide && Friction == EGSTN_MomentumSlideFriction::Character", ClampMin = "0.0"))
	float FrictionScale = 0.25f;

	/** Live frames (hitstop does not count) after which the slide stops whatever its speed, 0 = until it comes to rest or the move ends */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slide", meta = (EditCondition = "Mode == EGSTN_MomentumSlideMode::Slide", ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxFrames = 0;
};

/**
 * Per move data of the momentum slide (on FGSTN_CombatTimelineData). Off by default: a move without it behaves as before (a montage with
 * root motion stops the run on the first frame, one without lets the walking mode brake).
 *
 * How it plays (FGSTN_LayeredMove_MomentumSlide in GSTNCombatNP): on the first frame of the move the running velocity of the previous frame
 * is read in the ground plane; below MinSpeed nothing happens. Its direction is compared with the facing the move starts with (after a snap
 * turn of the same frame): a dot product at or above OppositeDotThreshold picks Along, below it picks Against. The chosen response then
 * slides the character on its feet along the running direction (never along the facing), on top of the montage's root motion, until the
 * speed reaches zero, MaxFrames pass, the move ends, is replaced or interrupted (a hit, a recoil), or the character leaves the ground.
 * Hitstop pauses it. Air moves are never affected.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_MomentumSlide
{
	GENERATED_BODY()

	/** Off: the move ignores the running speed (as every move did before) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide")
	bool bEnabled = false;

	/** Running speed (ground plane) below which the move ignores the momentum */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinSpeed = 150.f;

	/**
	 * Cosine of the angle between the running direction and the facing of the move. At or above it the move is "along" the momentum, below
	 * it "against" (-0.3 = more than about 107 degrees apart; 0 = anything behind the shoulders; -1 = never against)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide", meta = (EditCondition = "bEnabled", ClampMin = "-1.0", ClampMax = "1.0"))
	float OppositeDotThreshold = -0.3f;

	/** Started along the momentum: by default a long slide on the feet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide", meta = (EditCondition = "bEnabled"))
	FGSTN_MomentumSlideResponse Along;

	/** Started against the momentum (a turn-around attack): by default a short hard brake; Stop drops the speed at once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide", meta = (EditCondition = "bEnabled"))
	FGSTN_MomentumSlideResponse Against;

	/** The slide stops at a ledge instead of carrying the character off it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide", meta = (EditCondition = "bEnabled"))
	bool bStopAtLedge = true;

	/** The montage's own root motion is added to the slide; off = the slide alone moves the character while it lasts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Momentum Slide", meta = (EditCondition = "bEnabled"))
	bool bAddRootMotion = true;

	FGSTN_MomentumSlide()
	{
		// Against defaults to a hard brake: 600 cm/s rests in 0.12 s, about 36 cm
		Against.Deceleration = 5000.f;
	}
};

/** The friction terms of one slide step: Speed' = Speed - (Deceleration + Friction * Speed) * dt */
struct FGSTN_MomentumSlideBraking
{
	float Deceleration = 0.f;
	float Friction = 0.f;
};

/** Pure math of the momentum slide, shared by the Mover move and the tests. Vectors are in the ground plane of the caller's up */
struct GSTNCOMBATCORE_API FGSTN_MomentumSlideMath
{
	/**
	 * The response a move starts with, null when it ignores the momentum (disabled, too slow, or the picked response is Stop).
	 * FlatVelocity is the running velocity in the ground plane, FlatForward the unit facing in the same plane. bOutAgainst says which one
	 * was picked (also when it is a Stop)
	 */
	static const FGSTN_MomentumSlideResponse* PickResponse(const FGSTN_MomentumSlide& Slide, const FVector& FlatVelocity, const FVector& FlatForward, bool& bOutAgainst);

	/** The speed the slide starts with: the running speed scaled and capped by the response */
	static float StartSpeed(const FGSTN_MomentumSlideResponse& Response, float RunningSpeed);

	/** The friction of a response; the character's braking terms are only read by the Character friction */
	static FGSTN_MomentumSlideBraking ResolveBraking(const FGSTN_MomentumSlideResponse& Response, float CharacterDeceleration, float CharacterFriction);

	/** One step of the slide's speed, never below zero */
	static float StepSpeed(float Speed, const FGSTN_MomentumSlideBraking& Braking, float DeltaSeconds);

	/** Distance a slide covers before it rests or MaxFrames pass, at a fixed tick (a prediction for tuning and tests, never used to move anything) */
	static float PredictDistance(const FGSTN_MomentumSlideResponse& Response, float RunningSpeed, const FGSTN_MomentumSlideBraking& Braking, float TickRate, int32 MaxSteps = 1000);
};
