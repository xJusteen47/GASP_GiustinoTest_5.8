// Pure combat logic (no GAS, no Network Prediction): the Mario-style run stages (docs/notes/corsa-mario-piano.md, phase P1).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "GSTN_RunStageTypes.generated.h"

/** When the run boost command (L3 by default, in the kit's input data) may jump straight to BoostTargetStage (owner decision 3C) */
UENUM(BlueprintType)
enum class EGSTN_RunBoostMode : uint8
{
	/** Only within BoostWindowFrames of the start of the run (stick pushed from rest) */
	InitialWindow,
	/** Whenever the stick is pushed on the ground */
	Anytime,
	/** The command is ignored */
	Disabled
};

/** Where the carried speed came from (owner decision 2A; the skid source is filled by phase P2) */
UENUM(BlueprintType)
enum class EGSTN_RunCarrySource : uint8
{
	None,
	/** The stick was released: the speed of the frame before the release */
	StickRelease,
	/** A run skid started: the speed before the skid (P2) */
	Skid
};

/**
 * One run stage. Speeds are authored at the reference WalkSpeed stat and scaled by the stats factor like every other speed
 * (FGSTN_StatsMovementModifier), so a slow kit runs every stage slower.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunStage
{
	GENERATED_BODY()

	/** Top ground speed of the stage, at the reference WalkSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 450.f;

	/** Acceleration towards MaxSpeed, at the reference WalkSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 1800.f;

	/** How fast the velocity follows the stick direction (Mover's TurningStrength): lower = wider turns at the higher stages */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0"))
	float TurningStrength = 10.f;

	/** Braking with the stick released, at the reference WalkSpeed (strong at stage 1 = little slide) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float ReleaseBrake = 4000.f;

	/** Frames the stage must be held (stick forward, speed over the threshold) before the next stage. Ignored on the last stage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "1"))
	int32 FramesToNextStage = 40;

	/** Fraction of this stage's MaxSpeed the ground speed must reach for its frames to count towards the next stage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0", ClampMax = "1"))
	float NextStageSpeedFraction = 0.9f;

	/**
	 * Owner decision 2A: frames after the stick release during which a neutral ground attack still starts with the speed of before
	 * the release (read by the attacks in P3). 0 = never
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0"))
	int32 CarryFrames = 4;

	/**
	 * Air steering of a jump that took off at this stage (TakeoffCap air model): multiplies Mover's air control. Lower at the higher
	 * stages = a fast jump commits more to its direction (P2, owner decision 1C)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage", meta = (ClampMin = "0", ClampMax = "4"))
	float AirControlScale = 1.f;

	/**
	 * Acceleration ramp (owner 2026-11-16: "Mario / Astro Bot start, no step"): over this many frames from the start of the stage (stick
	 * pushed from rest for stage 1, the stage reached for the others) the acceleration grows from RampStartFraction of Acceleration to all
	 * of it, linearly. 0 = no ramp (full acceleration at once). Frames at 62.5 Hz
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage|Acceleration Ramp", meta = (ClampMin = "0", ClampMax = "120"))
	int32 AccelRampFrames = 0;

	/** Fraction of Acceleration on the first frame of the ramp */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage|Acceleration Ramp", meta = (ClampMin = "0", ClampMax = "1"))
	float RampStartFraction = 1.f;
};

/** The whole run tuning of a kit: the stages and the rules that move between them. Values only, copied out of the profile asset */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunStagesTuning
{
	GENERATED_BODY()

	/** Stage 1, 2, 3... (the owner's design has 3). Empty = run stages off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages")
	TArray<FGSTN_RunStage> Stages;

	/** Stick magnitude (0..1) under which the stick counts as released */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "0", ClampMax = "1"))
	float StickDeadzone = 0.25f;

	/**
	 * Stick magnitude from which a push counts as a start for the acceleration ramp. Small on purpose: the player's dead zone has already
	 * removed the noise, and a light tilt walking slowly must ramp too (StickDeadzone above is the run stages' "released")
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "0", ClampMax = "1"))
	float RampStickThreshold = 0.01f;

	/** Cosine between stick and ground velocity from which the stick counts as "forward" (stage frames count only then) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "-1", ClampMax = "1"))
	float ForwardStickCos = 0.7f;

	/** Cosine between stick and ground velocity under which the turn is tight and the stage drops */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "-1", ClampMax = "1"))
	float TightTurnCos = -0.2f;

	/** Stages lost on a tight turn (P2 replaces this with the skid above stage 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "1"))
	int32 TightTurnDropStages = 1;

	/** Frames with the stick released after which the run falls back to stage 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "0"))
	int32 ReleaseDropFrames = 8;

	/** A bump: the ground speed falls under this fraction of the previous frame's in one frame. Then the run falls back to stage 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "0", ClampMax = "1"))
	float BumpSpeedKeptFraction = 0.5f;

	/** Bumps are only checked above this ground speed (at the reference WalkSpeed), so stopping from a walk is no bump */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Rules", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float BumpMinSpeed = 300.f;

	/**
	 * The command that jumps to BoostTargetStage (Command.Movement.RunBoost, an ini tag; Tools/GAS/setup_run_stages.py sets it). The key
	 * (L3 by default) is in the kit's input data, never in code. Empty = no boost
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Boost", meta = (Categories = "Command"))
	FGameplayTag BoostCommand;

	/** When the boost command works (owner decision 3C: default = initial window) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Boost")
	EGSTN_RunBoostMode BoostMode = EGSTN_RunBoostMode::InitialWindow;

	/** InitialWindow: frames from the start of the run (stick pushed from rest) in which the boost works */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Boost", meta = (ClampMin = "0"))
	int32 BoostWindowFrames = 12;

	/**
	 * The stage the boost jumps to, counted FROM 1 like the owner names them: 1 = run, 2 = sprint 1, 3 = sprint 2. Owner 2026-11-17:
	 * L3 goes to the MIDDLE stage, not the last (was: always the last). Clamped into the table; a boost never lowers the stage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Boost", meta = (ClampMin = "1"))
	int32 BoostTargetStage = 2;

	/** BoostTargetStage as a 0-based index into Stages, clamped. The table must not be empty */
	int32 GetBoostTargetIndex() const { return FMath::Clamp(BoostTargetStage - 1, 0, Stages.Num() - 1); }

	/** The run skid (P2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Skid")
	FGSTN_RunSkidTuning Skid;

	/**
	 * P3, kit slipperiness (owner 10:12:59Z: "Luigi" slipperiness only as a kit setting, never the default): multiplies the braking of every
	 * attack momentum slide of this kit. 1 = the moves' own data, 0.3 = very slippery
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Attack Momentum", meta = (ClampMin = "0", ClampMax = "4"))
	float AttackSlideFrictionScale = 1.f;

	/** The air control (P2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Air")
	FGSTN_RunAirTuning Air;

	bool IsEnabled() const { return Stages.Num() > 0; }

	/** The stage of index Stage, clamped into the table. The table must not be empty */
	const FGSTN_RunStage& GetStage(int32 Stage) const { return Stages[FMath::Clamp(Stage, 0, Stages.Num() - 1)]; }

	/** The owner's three stages with the default rules (the values of a new profile asset) */
	static FGSTN_RunStagesTuning MakeDefault();
};

/**
 * The run state of one character, kept in Mover's sync state (FGSTN_RunStageSyncState wraps it), so a rollback restores it and a
 * re-simulation steps it again from the same values. Plain values only.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunStageState
{
	GENERATED_BODY()

	/** 0-based stage */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	uint8 Stage = 0;

	/** Frames counted towards the next stage (stick forward, speed over the threshold); reset when a frame does not qualify */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 FramesInStage = 0;

	/** Frames since the stick was pushed from rest on the ground, -1 while it is released */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 FramesSinceMoveStart = -1;

	/** Frames since the stick was released, -1 while it is pushed */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 FramesSinceStickRelease = -1;

	/** Speed kept for the attacks (decision 2A, and the skid momentum memory in P2), cm/s */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	float CarriedSpeed = 0.f;

	/** Frames since CarriedSpeed was stored, -1 = nothing carried */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 FramesSinceCarry = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	EGSTN_RunCarrySource CarrySource = EGSTN_RunCarrySource::None;

	/** Frames the carried speed stays valid, latched when it was stored (the stage may drop meanwhile) */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 CarryWindow = 0;

	/** Airborne at the end of the last stepped frame */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	bool bAirborne = false;

	/** Ground speed of the last grounded frame before the current air time (the TakeoffCap air top speed), cm/s */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	float TakeoffSpeed = 0.f;

	/** The stage at takeoff (its AirControlScale steers the air time) */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	uint8 TakeoffStage = 0;

	/** Ground speed at the end of the previous stepped frame (bump detection, the carried speed) */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	float PrevSpeed = 0.f;

	/** The simulation frame last stepped, so substeps of one frame step once; INDEX_NONE before the first */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 LastFrame = INDEX_NONE;

	/** Frames into the acceleration ramp of RampStage (-1 = stick at rest: the next push starts a ramp) */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 RampFrames = -1;

	/** The stage the ramp belongs to: a new stage starts its own ramp */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	uint8 RampStage = 0;

	/**
	 * External channel of the air momentum (FGSTN_AirMomentumLogic): the horizontal velocity a knockback, a launch or a bounce gave, cm/s,
	 * world space. Zero on the ground and after a player-owned override move
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	FVector ExternalVelocity = FVector::ZeroVector;

	bool operator==(const FGSTN_RunStageState& Other) const;
	bool operator!=(const FGSTN_RunStageState& Other) const { return !(*this == Other); }

	/** Compact network form: counters saturate at 1023 frames, speeds are rounded to 0.1 cm/s */
	void SerializeCompact(FArchive& Ar);
};

/** What one simulated frame tells the stage logic. Everything is from the frame being simulated (rollback safe) */
struct GSTNCOMBATCORE_API FGSTN_RunFrameInput
{
	int32 Frame = 0;
	bool bGrounded = false;
	/** Ground velocity at the end of the frame (world, the up component removed) */
	FVector GroundVelocity = FVector::ZeroVector;
	/** The move stick in world space (on the ground plane), magnitude 0..1 */
	FVector StickWorld = FVector::ZeroVector;
	/** The boost command was entered on this frame */
	bool bBoostCommand = false;
	/** A run skid started on this frame (P2): the stage drops and the speed of before the skid is carried */
	bool bSkidStarted = false;
	/** The ground speed the skid started from, cm/s */
	float SkidStartSpeed = 0.f;
	/**
	 * A run skid is live on this frame (its start frame included, until its exit phase is over). With Skid.bResumeStageAfterSkid the stage
	 * is frozen meanwhile: the stick held against the old velocity would otherwise read as a tight turn every frame and drop it
	 */
	bool bSkidActive = false;
	/** The stats speed factor of the frame (WalkSpeed / reference; 0 under a movement lock) */
	float SpeedFactor = 1.f;
};

/** The stage rules, pure: same input, same output on every machine */
struct GSTNCOMBATCORE_API FGSTN_RunStageLogic
{
	/** Steps State by one frame. A second call with the same Frame (a substep) changes nothing */
	static void Step(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunFrameInput& Input, FGSTN_RunStageState& State);

	/** The speed an attack starting now may carry (decision 2A): the carried speed while within its latched window, else 0 */
	static float GetCarriedSpeed(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State);

	/** TakeoffCap air top speed (before the stats factor): the takeoff speed with the profile's floor */
	static float GetAirTopSpeed(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, float Factor);

	/** Air control multiplier of the current air time: the takeoff stage's scale (TakeoffCap), 1 (Fixed) */
	static float GetAirControlScale(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State);

	/** Acceleration multiplier of the current stage's ramp (1 = no ramp or ramp done). The state is the one at the start of the frame */
	static float GetAccelerationRamp(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State);
};
