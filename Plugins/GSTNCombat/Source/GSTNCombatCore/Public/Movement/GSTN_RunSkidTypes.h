// Pure combat logic (no GAS, no Network Prediction): the run skid and the air control of the Mario-style run (docs/notes/corsa-mario-piano.md, P2).

#pragma once

#include "CoreMinimal.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid")
	bool bEnabled = true;

	/**
	 * Lowest run stage that skids, counted FROM 0 (0 = run, 1 = sprint 1, 2 = sprint 2). Unlike Boost Target Stage, which counts from 1: the
	 * display name says so (bugpass-1 B2, decision A: name only, the stored values keep their meaning). 1 = at the run a reversal is a quick
	 * pivot of the walking mode, the plan's rule
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", DisplayName = "Min Stage Index (0 = Run)"))
	int32 MinStage = 1;

	/** Ground speed (at the reference WalkSpeed) under which no skid starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinSpeed = 350.f;

	/** Cosine between stick and ground velocity at or under which the stick counts as reversed (-0.5 = more than 120 degrees apart) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "-1", ClampMax = "0"))
	float ReverseStickCos = -0.5f;

	/** Stick magnitude (0..1) needed to start a skid */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", ClampMax = "1"))
	float MinStickMagnitude = 0.5f;

	/** The skid's deceleration, a constant (cm/s^2, not scaled by the stats: a heavy kit gets its own value). 2600 stops 650 cm/s in 0.25 s, about 81 cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "1", ForceUnits = "cm/s^2"))
	float Brake = 2600.f;

	/** Frames of the Enter phase (they brake like Active, only the presentation differs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 EnterFrames = 4;

	/** Frames of the Exit phase after the skid comes to rest: the character turns to the new direction and pushes off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ExitFrames = 8;

	/** Speed along the new direction at the end of the Exit phase; the walking mode takes over from it (0 = from rest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ExitSpeed = 250.f;

	/**
	 * The Movement.State.Skid.JumpWindow tag is on from this skid frame (0 = the first) to JumpWindowEnd included, so a kit binding
	 * (a side somersault) can require it. Counted as the abilities see it: they read the state of the previous frame (CLAUDE.md rule 5),
	 * the tag compensates that frame
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid|Jump Window", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 JumpWindowStart = 2;

	/** Last skid frame with the jump window tag; -1 = until the skid ends */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid|Jump Window", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 JumpWindowEnd = -1;

	/** The kit's rule at a ledge; a surface with a SkidLedge other than Inherit wins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid")
	EGSTN_SkidLedge Ledge = EGSTN_SkidLedge::StopAtLedge;

	/** A skid drops the run back to stage 1 (Mario); off = the stage is kept. Ignored while bResumeStageAfterSkid is on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid")
	bool bResetsStage = true;

	/**
	 * Owner 2026-11-17 (default ON, to be confirmed by the owner): after the skid's turn the run pushes off from the stage it had when
	 * the skid started (no "cold start" from stage 1), so a skid back is not slower than releasing and pushing the stick again. The stage
	 * is frozen for the whole skid. Off = the old rule (bResetsStage). Lives in the run state (Mover sync state): rollback safe
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid")
	bool bResumeStageAfterSkid = true;

	/** Frames after the skid started in which a neutral ground attack still carries the speed of before the skid (P3 momentum memory) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 CarryFrames = 6;

	/**
	 * PLACEHOLDER presentation until the real skid clips arrive with the anim layer plan: the torso leans back against the slide by this
	 * angle through the directional additive node (degrees, 0 = none)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Skid|Presentation", meta = (ClampMin = "0", ClampMax = "60"))
	float PlaceholderLeanDeg = 22.f;
};

/** Which air control model a kit uses (owner decision 1C: both, per kit) */
UENUM(BlueprintType)
enum class EGSTN_AirControlModel : uint8
{
	/** Default: the air top speed is the takeoff speed (with a floor), the steering is the takeoff stage's AirControlScale */
	TakeoffCap,
	/** The old behaviour: the walking top speed and Mover's fixed air control, scaled by the AirControl stat only */
	Fixed
};

/** The air half of the run tuning */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_RunAirTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Air")
	EGSTN_AirControlModel Model = EGSTN_AirControlModel::TakeoffCap;

	/** TakeoffCap: the air top speed never goes under this (at the reference WalkSpeed), so a standing jump can still drift */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Air", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MinAirSpeed = 300.f;

	/** The AirControl stat multiplies Mover's air control (the stat was declared but read by nothing before P2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Air")
	bool bUseAirControlStat = true;

	/**
	 * Air momentum (owner 2026-11-18): player drift and external pushes in two channels, the neutral stick style (Astro default, Mario
	 * option) and stronger air steering. Style MoverLegacy = the falling mode as before
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Air", meta = (ShowOnlyInnerProperties))
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
};
