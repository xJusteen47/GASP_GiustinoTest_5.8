// Pure combat logic (no GAS, no Network Prediction): the run skid and the air control of the Mario-style run (docs/notes/corsa-mario-piano.md, P2).

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Movement/GSTN_AirMomentumTypes.h"
#include "GSTN_RunSkidTypes.generated.h"

/** What a run skid does at a ledge (owner 2026-11-16 17:46Z). A surface may override it (FGSTN_SurfaceLocomotion::SkidLedge) */
UENUM(BlueprintType)
enum class EGSTN_SkidLedge : uint8
{
	/** The skid stops at the edge (default) */
	StopAtLedge,
	/** The skid carries the character off the edge (a slippery character on purpose) */
	SlideOff
};

/**
 * Which way the body turns in the skid's exit when the new direction is (nearly) straight behind it (anim-setup-1). An exact reverse has no
 * shorter side, so without a rule the side came from float noise and the body turned against the skid clip half the time. Set it to the side
 * the kit's skid clip turns; turns clearly off the reverse still take the shorter way.
 */
UENUM(BlueprintType)
enum class EGSTN_SkidTurnSide : uint8
{
	/** Always the shorter way (the old behaviour: a straight reverse picks a side at random) */
	Shortest,
	/** Positive yaw seen from above: forward turns toward the right hand (UE +Y) */
	Right,
	/** Negative yaw: forward turns toward the left hand */
	Left
};

/** The phase of a run skid, written by the simulation, read by the presentation (anim layer, foot slide VFX) */
UENUM(BlueprintType)
enum class EGSTN_RunSkidPhase : uint8
{
	None,
	/** First frames: the body plants the feet */
	Enter,
	/** Braking on the feet along the old running direction */
	Active,
	/** At rest: the turn to the new direction and the push off */
	Exit
};

/**
 * How the run skid moves the capsule (skid-physics-1, owner 2026-11-21: "the skid is locked instead of an organic change of speed").
 * Every model brakes along the old running direction and pushes off toward the stick; they differ in who decides the velocity.
 */
UENUM(BlueprintType)
enum class EGSTN_RunSkidMotion : uint8
{
	/** Default. The velocity is physics: from the run speed of the frame it brakes continuously (BrakeDecel, BrakeCurve, a short onset so
	 *  the brake bites without a jolt), comes through zero and accelerates toward the stick (PushAccel, PushCurve) up to PushSpeed, where
	 *  the walking mode takes over at the same speed. The facing follows that progress. The animation follows the capsule, never the reverse */
	Physics,
	/** skid-fix-1 (2026-11-20): the capsule follows the skid clip's baked root (RootTrack + TurnCurve) for the whole clip, stick ignored.
	 *  Falls back to BrakeExit when the clip root is not baked */
	ClipRoot UMETA(DisplayName = "Clip Root (Legacy)"),
	/** The first version (P2): constant Brake to rest, then a linear push to ExitSpeed over ExitFrames */
	BrakeExit UMETA(DisplayName = "Brake Exit (Legacy)")
};

/** The Physics motion of the run skid. Speeds and accelerations at the reference WalkSpeed, frames at the 62.5 Hz fixed tick */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunSkidPhysicsTuning
{
	GENERATED_BODY()

	/** Braking deceleration along the old running direction, cm/s^2 (times BrakeCurve, times the floor's FrictionScale). 3600 stops a
	 *  472 cm/s run in ~0.15 s over ~35 cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "1", ForceUnits = "cm/s^2"))
	float BrakeDecel = 3600.f;

	/** Frames over which the brake grows from 0 to full: the run had no deceleration the frame before, a full brake at once reads as a hit
	 *  wall. 0 = full brake from the first frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "0", ClampMax = "30", ForceUnits = "Frames"))
	int32 BrakeOnsetFrames = 3;

	/** Shape of the brake: X = speed left / speed at the skid start (1 at the start, 0 at rest), Y = multiplier of BrakeDecel (values under
	 *  0.05 count as 0.05 so the skid always comes to rest). No keys = 1 everywhere (constant deceleration, like kinetic friction) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics")
	FRuntimeFloatCurve BrakeCurve;

	/** Acceleration of the push-off toward the stick, cm/s^2 (times PushCurve, times the floor's FrictionScale) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "1", ForceUnits = "cm/s^2"))
	float PushAccel = 3000.f;

	/** Shape of the push: X = speed / PushSpeed (0 at rest, 1 at the hand-off), Y = multiplier of PushAccel (min 0.05). No keys = 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics")
	FRuntimeFloatCurve PushCurve;

	/** Speed of the push at which the skid ends and the walking mode (the run stages, the reversal boost) carries on from the same velocity.
	 *  0 = no push: the skid ends at rest */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float PushSpeed = 420.f;

	/**
	 * The facing turns on the skid's progress, not on a clock: progress 0 = the skid start, 1 = at rest, 2 = the push at PushSpeed. The turn
	 * runs (smoothstep) from TurnStartProgress to TurnEndProgress, so a longer slide turns later on its own. Defaults: the body turns while
	 * it brakes and faces the new way when it comes to rest
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "0", ClampMax = "2"))
	float TurnStartProgress = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "0", ClampMax = "2"))
	float TurnEndProgress = 1.f;

	/** The stick back along the old direction during the brake (cosine at or over this) ends the skid: the walking mode runs on from the
	 *  speed left. -1 = never */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "-1", ClampMax = "1"))
	float ForwardCancelCos = 0.5f;

	/** Stick magnitude under which there is no push: a skid with the stick released ends at rest */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "0", ClampMax = "1"))
	float PushStickMin = 0.2f;

	/** Safety cap of each half (brake, push), frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ClampMin = "1", ClampMax = "240", ForceUnits = "Frames"))
	int32 MaxFrames = 90;
};

/**
 * The run skid of a kit (Mario-style run P2): the stick pushed against a fast run brakes the character on its feet along the old
 * direction with a real deceleration, then it turns and pushes off the other way. Below the threshold (stage 1, or a slow walk) the
 * walking mode's own tight turn handles it, no skid.
 *
 * Everything is in simulation frames at the fixed tick, so the phases are a pure function of the frame and the start speed.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunSkidTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid", meta = (DisplayName = "Skid Enabled"))
	bool bEnabled = true;

	/** Who moves the capsule in the skid (skid-physics-1). Console override: gstn.Skid.Motion */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid")
	EGSTN_RunSkidMotion Motion = EGSTN_RunSkidMotion::Physics;

	/** The Physics motion's values (Motion = Physics) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Physics", meta = (ShowOnlyInnerProperties))
	FGSTN_RunSkidPhysicsTuning Physics;

	/** Lowest 0-based run stage that skids (1 = stage 2: at stage 1 a reversal is a quick pivot of the walking mode, the plan's rule) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Start", meta = (ClampMin = "0"))
	int32 MinStage = 1;

	/** Ground speed (at the reference WalkSpeed) under which no skid starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Start", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinSpeed = 350.f;

	/** Cosine between stick and ground velocity at or under which the stick counts as reversed (-0.5 = more than 120 degrees apart) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Start", meta = (ClampMin = "-1", ClampMax = "0"))
	float ReverseStickCos = -0.5f;

	/** Stick magnitude (0..1) needed to start a skid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Start", meta = (ClampMin = "0", ClampMax = "1"))
	float MinStickMagnitude = 0.5f;

	/** The skid's deceleration, a constant (cm/s^2, not scaled by the stats: a heavy kit gets its own value). 2600 stops 650 cm/s in 0.25 s, about 81 cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy|Skid", meta = (EditCondition = "Motion != EGSTN_RunSkidMotion::Physics", ClampMin = "1", ForceUnits = "cm/s^2"))
	float Brake = 2600.f;

	/** Frames of the Enter phase (they brake like Active, only the presentation differs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Phases", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 EnterFrames = 4;

	/** Frames of the Exit phase after the skid comes to rest: the character turns to the new direction and pushes off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Phases", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ExitFrames = 8;

	/** Speed along the new direction at the end of the Exit phase; the walking mode takes over from it (0 = from rest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy|Skid", meta = (EditCondition = "Motion != EGSTN_RunSkidMotion::Physics", ClampMin = "0", ForceUnits = "cm/s"))
	float ExitSpeed = 250.f;

	/** Side of the exit turn for a near-reverse; must match the skid clip's turn (SS_180_Turn_Root180 turns +180 yaw = Right) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Turn")
	EGSTN_SkidTurnSide TurnSide = EGSTN_SkidTurnSide::Right;

	/** A new direction within this many degrees of straight behind counts as a reverse and turns on TurnSide */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Turn", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "Degrees"))
	float ReverseConeDeg = 45.f;

	/**
	 * The body's turn follows the skid clip (anim-setup-2): share of the whole turn (0..1) reached at each sample of skid time, from the
	 * skid's first frame, TurnCurveRate samples a second. Baked from the clip's root yaw (Tools/GAS/skid_turn_curve_bake.py; the clip plays
	 * at 1x from the skid start), so the capsule turns when the clip turns and a planted foot never has to break its pin. The brake turns
	 * along it; the exit finishes whatever is left by its last frame. Empty = the old rule: facing held in the brake, a linear turn in
	 * the exit frames.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy|Skid", meta = (EditCondition = "Motion != EGSTN_RunSkidMotion::Physics"))
	TArray<float> TurnCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy|Skid", meta = (EditCondition = "Motion != EGSTN_RunSkidMotion::Physics", ClampMin = "1", ForceUnits = "Hz"))
	float TurnCurveRate = 30.f;

	/** TurnCurve at Seconds after the skid start, linear between samples, 1 past the end; -1 when there is no curve */
	float SampleTurnCurve(float Seconds) const
	{
		if (TurnCurve.Num() == 0)
		{
			return -1.f;
		}
		const float X = FMath::Max(Seconds, 0.f) * FMath::Max(TurnCurveRate, 1.f);
		const int32 I = FMath::FloorToInt32(X);
		if (I >= TurnCurve.Num() - 1)
		{
			return 1.f;
		}
		return FMath::Clamp(FMath::Lerp(TurnCurve[I], TurnCurve[I + 1], X - static_cast<float>(I)), 0.f, 1.f);
	}

	/**
	 * skid-fix-1 (owner 2026-11-20, stock GASP: the capsule never turns or moves toward the stick while a pivot clip plays its plant): the skid
	 * clip's root path, cm from the clip's first frame, X along the old running direction, Y to the right of it (the clip's own turn side),
	 * TurnCurveRate samples a second. Baked with TurnCurve (Tools/GAS/skid_turn_curve_bake.py). With TurnCurve and RootTrack both set the
	 * skid lasts the clip: the capsule moves on this path and turns on TurnCurve from the first frame to the clip's end, with no brake
	 * formula and no exit push toward the stick; the walking mode takes the stick back after it. Empty = the brake + exit rule above.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Legacy|Skid", meta = (EditCondition = "Motion != EGSTN_RunSkidMotion::Physics"))
	TArray<FVector2D> RootTrack;

	/** True when the skid follows its clip's root (RootTrack and TurnCurve both baked) */
	bool FollowsClipRoot() const
	{
		return RootTrack.Num() >= 2 && TurnCurve.Num() >= 2;
	}

	/** Length of the baked clip root path, seconds */
	float RootTrackSeconds() const
	{
		return RootTrack.Num() >= 2 ? static_cast<float>(RootTrack.Num() - 1) / FMath::Max(TurnCurveRate, 1.f) : 0.f;
	}

	/** RootTrack at Seconds after the skid start, linear between samples, held at the ends */
	FVector2D SampleRootTrack(float Seconds) const
	{
		if (RootTrack.Num() == 0)
		{
			return FVector2D::ZeroVector;
		}
		const float X = FMath::Max(Seconds, 0.f) * FMath::Max(TurnCurveRate, 1.f);
		const int32 I = FMath::FloorToInt32(X);
		if (I >= RootTrack.Num() - 1)
		{
			return RootTrack.Last();
		}
		return FMath::Lerp(RootTrack[I], RootTrack[I + 1], X - static_cast<float>(I));
	}

	/**
	 * The Movement.State.Skid.JumpWindow tag is on from this skid frame (0 = the first) to JumpWindowEnd included, so a kit binding
	 * (a side somersault) can require it. Counted as the abilities see it: they read the state of the previous frame (CLAUDE.md rule 5),
	 * the tag compensates that frame
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Jump Window", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 JumpWindowStart = 2;

	/** Last skid frame with the jump window tag; -1 = until the skid ends */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Jump Window", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 JumpWindowEnd = -1;

	/** The kit's rule at a ledge; a surface with a SkidLedge other than Inherit wins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid")
	EGSTN_SkidLedge Ledge = EGSTN_SkidLedge::StopAtLedge;

	/**
	 * Owner 2026-11-17 (default ON, to be confirmed by the owner): after the skid's turn the run pushes off from the stage it had when
	 * the skid started (no "cold start" from stage 1), so a skid back is not slower than releasing and pushing the stick again. The stage
	 * is frozen for the whole skid. Off = the skid drops the run back to stage 1 (Mario). Lives in the run state (Mover sync state): rollback safe
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Run Stage")
	bool bResumeStageAfterSkid = true;

	/** Frames after the skid started in which a neutral ground attack still carries the speed of before the skid (P3 momentum memory) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Run Stage", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 CarryFrames = 6;

	/**
	 * PLACEHOLDER presentation until the real skid clips arrive with the anim layer plan: the torso leans back against the slide by this
	 * angle through the directional additive node (degrees, 0 = none)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid|Presentation", meta = (ClampMin = "0", ClampMax = "60"))
	float PlaceholderLeanDeg = 22.f;
};

/** Which air control model a kit uses (owner decision 1C: both, per kit) */
UENUM(BlueprintType)
enum class EGSTN_AirControlModel : uint8
{
	/** Default: the air top speed is the takeoff speed (with a floor), the steering is the takeoff stage's AirControlScale */
	TakeoffCap,
	/** The old behaviour: the walking top speed and Mover's fixed air control, scaled by the AirControl stat only */
	Fixed UMETA(DisplayName = "Fixed (Legacy)")
};

/** The air half of the run tuning */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunAirTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Control", meta = (DisplayName = "Air Control Model"))
	EGSTN_AirControlModel Model = EGSTN_AirControlModel::TakeoffCap;

	/** TakeoffCap: the air top speed never goes under this (at the reference WalkSpeed), so a standing jump can still drift */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Control", meta = (EditCondition = "Model == EGSTN_AirControlModel::TakeoffCap", ClampMin = "0", ForceUnits = "cm/s"))
	float MinAirSpeed = 300.f;

	/** The AirControl stat multiplies Mover's air control (the stat was declared but read by nothing before P2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Control")
	bool bUseAirControlStat = true;

	/**
	 * Air momentum (owner 2026-11-18): player drift and external pushes in two channels, the neutral stick style (Astro default, Mario
	 * option) and stronger air steering. Style MoverLegacy = the falling mode as before
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Momentum", meta = (ShowOnlyInnerProperties))
	FGSTN_AirMomentumTuning Momentum;
};

/** Pure math of the skid, shared by the Mover move, the combat tick and the tests */
struct GSTNCOMBATCORE_API FGSTN_RunSkidMath
{
	/**
	 * True when a skid starts now. Stage: the 0-based run stage; GroundVelocity and StickWorld in the ground plane (stick magnitude 0..1);
	 * Factor = the stats speed factor (thresholds are authored at the reference WalkSpeed)
	 */
	static bool ShouldStart(const FGSTN_RunSkidTuning& Tuning, int32 Stage, const FVector& GroundVelocity, const FVector& StickWorld, float Factor);

	/** Speed of a skid live frame (0 = the first), constant deceleration: closed form, no accumulation drift */
	static float SpeedAt(float StartSpeed, float Brake, int32 LiveFrame, float DeltaSeconds);

	/** First live frame at which the speed is 0 */
	static int32 FramesToRest(float StartSpeed, float Brake, float DeltaSeconds);

	/** The phase of LiveFrame; RestFrame = the live frame the skid came to rest (a ledge may stop it early). None once the exit is over */
	static EGSTN_RunSkidPhase PhaseAt(const FGSTN_RunSkidTuning& Tuning, int32 LiveFrame, int32 RestFrame);

	/** The jump window tag at LiveFrame (already shifted by the one frame the abilities see late) */
	static bool InJumpWindow(const FGSTN_RunSkidTuning& Tuning, int32 LiveFrame);

	/** Stopping distance of a skid (tuning aid and the anim layer's distance matching) */
	static float StopDistance(float StartSpeed, float Brake);

	/** A shape curve at X; 1 when it has no keys, never under 0.05 (a skid always reaches rest and the hand-off) */
	static float EvalShape(const FRuntimeFloatCurve& Curve, float X);

	/**
	 * Physics motion, the brake: ground speed along the old direction at the END of live frame LiveFrame (0 = the first), integrated frame by
	 * frame from StartSpeed (pure function of the arguments: a rollback or a refunded substep gets the same value). 0 from the rest frame on
	 */
	static float PhysicsBrakeSpeedAt(const FGSTN_RunSkidPhysicsTuning& Physics, float StartSpeed, float BrakeDecel, int32 LiveFrame, float DeltaSeconds);

	/** Physics motion: frames of the brake, the last one ends at 0 (capped at MaxFrames) */
	static int32 PhysicsFramesToRest(const FGSTN_RunSkidPhysicsTuning& Physics, float StartSpeed, float BrakeDecel, float DeltaSeconds);

	/** Physics motion, the push: speed toward the new direction at the END of push frame PushFrame (0 = the first after rest) */
	static float PhysicsPushSpeedAt(const FGSTN_RunSkidPhysicsTuning& Physics, float PushAccel, float PushSpeed, int32 PushFrame, float DeltaSeconds);

	/** Physics motion: frames of the push, the last one reaches PushSpeed (0 when PushSpeed is 0; capped at MaxFrames) */
	static int32 PhysicsPushFrames(const FGSTN_RunSkidPhysicsTuning& Physics, float PushAccel, float PushSpeed, float DeltaSeconds);

	/** Share (0..1) of the whole turn at a skid progress (0 start, 1 rest, 2 hand-off) */
	static float PhysicsTurnShare(const FGSTN_RunSkidPhysicsTuning& Physics, float Progress);
};
