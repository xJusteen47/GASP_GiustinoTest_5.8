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

	/**
	 * Readable stage step (owner 2026-11-20, decision 1A, docs/notes/run-stages-leggibili-piano.md): when the run CLIMBS into this stage
	 * (FramesToNextStage of the stage below, not a boost or a drop) the acceleration is multiplied by EntryAccelMultiplier for this many
	 * frames, so the new top speed comes in a few frames, in the same frame as the clip and the effects (a step, not a ramp). A stage with
	 * a burst has no acceleration ramp while the readable steps are on (gstn.RunStage.Readable): the ramp stays for the start from rest
	 * (stage 1). 0 = off (the stage keeps its ramp, the old continuous run). Frames at 62.5 Hz
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage|Readable Step", meta = (ClampMin = "0", ClampMax = "60"))
	int32 EntryBurstFrames = 0;

	/** Multiplier of this stage's Acceleration during the entry burst. Mover still stops at MaxSpeed, so a large value only shortens it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage|Readable Step", meta = (ClampMin = "1", ClampMax = "30"))
	float EntryAccelMultiplier = 8.f;

	/**
	 * Announcement of the NEXT stage (owner decision 2A, sprint 2: heel sparks and a deepening lean for ~0.33 s): the last TellFrames frames
	 * this stage counts towards FramesToNextStage expose "tell live" (FGSTN_RunStageLogic::IsStageTell), for the presentation (VFX, lean).
	 * Simulation side it changes nothing: no slow-down, the speed stays the stage's. Lives on the stage being LEFT (on stage 2 for the sprint
	 * 2 tell). Ends without a change if the run stops qualifying. 0 = no tell. Ignored on the last stage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage|Readable Step", meta = (ClampMin = "0", ClampMax = "120"))
	int32 TellFrames = 0;

	/**
	 * Charge pose before the next stage (decision 2A: ~0.1 s throw-forward pose, the anticipation before the burst): the last ChargeFrames
	 * frames before the stage change expose "charge live" (IsStageCharge), the clip's cue. Usually inside the tell (ChargeFrames <= TellFrames).
	 * Presentation only, like the tell. 0 = no charge pose. Ignored on the last stage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stage|Readable Step", meta = (ClampMin = "0", ClampMax = "60"))
	int32 ChargeFrames = 0;
};

/**
 * Run start stamp (owner 2026-11-18, choice B, Toad reference): pushed hard from (almost) still, the character first stamps almost in
 * place, limbs already frantic, for Frames, then accelerates as usual. Simulation side only: the acceleration is cut to AccelFraction
 * meanwhile. The frantic clip is presentation (it reads IsStartStamp / the counter). Ends early when the stick goes back to rest, the
 * stage changes (boost) or the character leaves the ground. Frames at 62.5 Hz
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunStartStampTuning
{
	GENERATED_BODY()

	/** Frames of the stamp. 0 = off (Toad: ~0.33 s = 21 frames) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start Stamp", meta = (ClampMin = "0", ClampMax = "120"))
	int32 Frames = 0;

	/** Fraction of the stage acceleration during the stamp: small = almost in place */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start Stamp", meta = (ClampMin = "0", ClampMax = "1"))
	float AccelFraction = 0.05f;

	/** Stick magnitude (0..1) that starts the stamp: a light tilt walks without it (owner: walk = light tilt only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start Stamp", meta = (ClampMin = "0", ClampMax = "1"))
	float StickMin = 0.5f;

	/** The stamp starts only under this ground speed (at the reference WalkSpeed): a re-push while running never stamps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start Stamp", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxStartSpeed = 60.f;
};

/**
 * Boost dash (owner 2026-11-19: "L3 should give Smash's dash"): the boost command skips the start stamp and launches at once, to get away
 * from attacks. On the boost the stamp ends and the acceleration is multiplied for Frames, so the new stage's top speed comes in a few
 * frames instead of a ramp. Frames at 62.5 Hz
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunBoostDashTuning
{
	GENERATED_BODY()

	/** Frames of the burst. 0 = off (the boost only changes the stage, as before) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost Dash", meta = (ClampMin = "0", ClampMax = "60"))
	int32 Frames = 0;

	/** Multiplier of the stage acceleration during the burst (replaces the ramp) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost Dash", meta = (ClampMin = "1", ClampMax = "30"))
	float AccelMultiplier = 8.f;
};

/**
 * Run reversal (skid-reversal-1, owner 2026-11-20, Toad reference): the stick flipped against a run must give back a run the other way in about
 * 0.3 s; with the stage values alone it takes ~0.55 s (the walking mode's TurningStrength eats the old velocity, then the stage acceleration
 * builds the new one). From the frame the stick goes against the ground velocity until the speed along the stick reaches EndSpeedFraction of
 * the stage top speed, the stage acceleration is multiplied and the walking mode's turning strength is raised. Same walking mode, same
 * properties (stock GASP has no reversal-specific braking: a 180 is just its walking parameters), only stronger for a few frames. Simulation
 * side only; the pivot clip is the chooser's. Frames at 62.5 Hz
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunReversalTuning
{
	GENERATED_BODY()

	/** Off = a reversal uses the stage values like any turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal")
	bool bEnabled = false;

	/** Cosine between stick and ground velocity under which the stick counts as reversed (-0.5 = more than 120 degrees off) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "-1", ClampMax = "0"))
	float StickCos = -0.5f;

	/** Stick magnitude (0..1) needed: a light tilt back is a slow turn, not a reversal */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "0", ClampMax = "1"))
	float MinStickMagnitude = 0.5f;

	/** Ground speed (at the reference WalkSpeed) from which a flip is a reversal: a walk turns as usual */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinSpeed = 250.f;

	/**
	 * The walking mode's TurningStrength during the reversal (the larger of this and the stage's). It sets how fast the old velocity dies:
	 * higher = shorter slide. Mover reads 10 as strong, 100 as near instant
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "0", ClampMax = "200"))
	float TurningStrength = 30.f;

	/** Multiplier of the stage acceleration during the reversal (replaces the stage's acceleration ramp). It sets how fast the run comes back */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "1", ClampMax = "10"))
	float AccelerationMultiplier = 2.f;

	/** The reversal ends when the speed along the stick reaches this fraction of the stage top speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "0.1", ClampMax = "1"))
	float EndSpeedFraction = 0.9f;

	/** The reversal never lasts more than this many frames (a wall, a slope) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reversal", meta = (ClampMin = "1", ClampMax = "120"))
	int32 MaxFrames = 40;
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

	/** The run start stamp (owner choice B, 2026-11-18) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Start Stamp")
	FGSTN_RunStartStampTuning StartStamp;

	/** The boost dash: L3 skips the stamp and launches at once (owner 2026-11-19) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Boost")
	FGSTN_RunBoostDashTuning BoostDash;

	/** The run skid (P2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Skid")
	FGSTN_RunSkidTuning Skid;

	/** The run reversal: how fast a stick flipped against the run gives a run the other way (skid-reversal-1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stages|Reversal")
	FGSTN_RunReversalTuning Reversal;

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
	 * Run start stamp: -1 = none this push (the stick at rest resets it), else frames since the stamp started. The stamp is live while
	 * it is under StartStamp.Frames; past it, it stays counting until the stick rests, so one push stamps once
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 StampFrames = -1;

	/** Boost dash: -1 = none (the stick at rest resets it), else frames since the boost; the burst is live while under BoostDash.Frames */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 DashFrames = -1;

	/** Run reversal: -1 = none, else frames since the stick went against the run (live until the run the other way is back, Reversal) */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 ReverseFrames = -1;

	/** Readable stage step: -1 = none, else frames since the run climbed into the current stage (the entry burst is live while under its
	 *  EntryBurstFrames). Reset by a boost, a drop, a skid and the stick at rest */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 EntryFrames = -1;

	/** The simulation frame the current stage began (any cause: climb, boost, drop, reset), INDEX_NONE before the first change. With
	 *  LastFrame it gives the time in the stage (presentation: the stage clip's start, the burst VFX) */
	UPROPERTY(BlueprintReadOnly, Category = "Run Stages")
	int32 StageEnteredFrame = INDEX_NONE;

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
	/** The readable stage steps are on (cvar gstn.RunStage.Readable, read by the NP adapter; same value on every machine). Off = no entry burst */
	bool bReadableSteps = true;
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

	/** The start stamp is live in State (the end of the last stepped frame) */
	static bool IsStartStamp(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State);

	/** The boost dash burst is live in State */
	static bool IsBoostDash(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State);

	/** A run reversal is live in State: its acceleration multiplier and turning strength apply on the next frame */
	static bool IsReversing(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State);

	/**
	 * Acceleration multiplier of the current stage's ramp (1 = no ramp or ramp done). The state is the one at the start of the frame.
	 * bReadableSteps (gstn.RunStage.Readable): the entry burst replaces the ramp of a stage that has one (decision 1A); off = the old ramps
	 */
	static float GetAccelerationRamp(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps = true);

	/** The entry burst of the current stage is live in State (readable step, FGSTN_RunStage::EntryBurstFrames) */
	static bool IsStageEntryBurst(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps = true);

	/** The tell of the next stage is live in State (FGSTN_RunStage::TellFrames of the current stage): grounded, qualifying, near the change */
	static bool IsStageTell(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps = true);

	/** Progress of the tell, (0, 1]: 1 on the frame before the change (the lean deepens with it); 0 when no tell is live */
	static float GetStageTellAlpha(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps = true);

	/** The charge pose before the next stage is live in State (FGSTN_RunStage::ChargeFrames of the current stage) */
	static bool IsStageCharge(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps = true);
};
