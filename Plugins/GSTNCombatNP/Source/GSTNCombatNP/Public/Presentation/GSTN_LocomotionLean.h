// Combat presentation (GAS-NPP fork): additive locomotion lean and the locomotion animation profile (docs/notes/additive-lean-1.md). Never read by
// the simulation.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_AnimSetTypes.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "GSTN_LocomotionLean.generated.h"

/**
 * The additive lean while moving (owner 2026-11-18, "Mario / Astro Bot" feel): the torso leans into the acceleration of the presented
 * velocity, so into a turn (the centripetal part, speed x turn rate), forward when speeding up and back when braking. It bends the spine on
 * top of GASP's motion matched pose (the legs, the root and the foot lock keep GASP's pose), through the directional additive node already in
 * the main AnimGraph. Angles are summed over the spine chain (spine_01..head).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_LocomotionLeanSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean")
	bool bEnabled = true;

	/** Largest lean, degrees summed over the spine chain */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "45.0", ForceUnits = "deg"))
	float MaxAngleDeg = 14.f;

	/** Horizontal acceleration that reaches MaxAngleDeg (cm/s^2). Lower = leans more easily */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "100.0", ForceUnits = "cm/s^2"))
	float AccelForMaxLean = 3000.f;

	/** Share of the turn (sideways) part: 1 = full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TurnScale = 1.f;

	/** Share of the speed up / brake (along the run) part: GASP's start and stop clips already lean, so less than the turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SpeedChangeScale = 0.5f;

	/** No lean under this speed, full lean from FullLeanSpeed (cm/s): standing turns do not tilt the body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinSpeed = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float FullLeanSpeed = 350.f;

	/** Scale per run stage (index = stage: run, sprint 1, sprint 2...; a missing stage uses the last entry) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean")
	TArray<float> StageScale = { 1.f, 1.15f, 1.3f };

	/** Smoothing of the measured acceleration (s): the presented velocity steps at the 62.5 Hz simulation rate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|Feel", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AccelSmoothingSeconds = 0.06f;

	/** The lean follows its target as a spring: frequency (Hz, higher = snappier) and damping (1 = no overshoot, lower = follow-through) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|Feel", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float SpringFrequency = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|Feel", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float SpringDamping = 0.65f;

	/** Scale in the air (0 = the lean springs back to straight after take off) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|States", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirScale = 0.f;

	/** Scale during a move timeline (attacks): 0 = the attack's own pose is untouched */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|States", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackScale = 0.f;

	/** Scale during a run skid or an attack slide (the skid has its own lean and clip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|States", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideScale = 0.f;

	float GetStageScale(int32 Stage) const
	{
		return StageScale.Num() == 0 ? 1.f : StageScale[FMath::Clamp(Stage, 0, StageScale.Num() - 1)];
	}
};

class UAnimSequenceBase;
class UAnimInstance;

/**
 * Run start "scramble" (owner 2026-11-18, decision 1A, docs/notes/mosse/run-start-stop-skid-brief.md section 1): from still, the run pose
 * from the first frames, the run cycle played fast (rate 1 + K x (1 - speed / target)) while the speed builds, the feet slipping on purpose
 * (an override layer: GASP's stride warping and start clips are not in the pose), and an extra forward lean that springs back with a small
 * rebound. Presentation only. Played by ABP_GSTN_RunStartLayer (UGSTN_RunStartAnimInstance).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_RunStartScrambleSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start")
	bool bEnabled = true;

	/** The looping run cycle the scramble plays (root motion unused) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start")
	TSoftObjectPtr<UAnimSequenceBase> RunCycle =
		TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F.M_Neutral_Run_Loop_F")));

	/** K: play rate = 1 + K x (1 - speed / target) x stick factor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float PlayRateBoost = 0.9f;

	/** Starts when the ground speed crosses this upwards on the ground (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float StartFromSpeed = 30.f;

	/** Ends when the speed reaches this share of the run stage's max speed (brief: ~90%) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float EndSpeedFraction = 0.9f;

	/** Stick factor: 0 under MinStick (a walk start, no scramble), 1 from FullStick */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinStick = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FullStick = 0.8f;

	/** Extra forward lean on top of the run lean at the start, scaled like the play rate (degrees over the spine chain) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start", meta = (ClampMin = "0.0", ClampMax = "30.0", ForceUnits = "deg"))
	float ExtraLeanDeg = 11.f;

	/** The extra lean follows its target as a spring: under 1 damping it rebounds past the run lean once (brief: ~2 deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start|Feel", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float LeanSpringFrequency = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start|Feel", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float LeanSpringDamping = 0.55f;

	/** Near cut from idle to the run pose (brief: 2 frames), and the blend back to GASP's run at the end */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start|Feel", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float BlendInSeconds = 0.032f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Start|Feel", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float BlendOutSeconds = 0.12f;

	float StickFactor(float Stick) const
	{
		return FullStick > MinStick ? FMath::Clamp((Stick - MinStick) / (FullStick - MinStick), 0.f, 1.f) : (Stick >= MinStick ? 1.f : 0.f);
	}
};

/** The scramble between frames, game thread. Pure but for the inputs: the tests drive it */
struct GSTNCOMBATNP_API FGSTN_RunStartScrambleState
{
	bool bActive = false;
	/** 1 - speed / target, times the stick factor: drives the play rate and the extra lean */
	float Alpha = 0.f;
	float ClipTime = 0.f;
	float PreviousSpeed = 0.f;
	/** The extra lean (degrees, along the velocity; negative = the rebound) and its rate */
	float LeanDeg = 0.f;
	float LeanRate = 0.f;

	float PlayRate(const FGSTN_RunStartScrambleSettings& Settings) const { return 1.f + Settings.PlayRateBoost * Alpha; }

	/** One presented frame. TargetSpeed: the run stage's max speed; Stick: 0..1 */
	void Step(const FGSTN_RunStartScrambleSettings& Settings, float DeltaSeconds, float Speed, float TargetSpeed, float Stick, bool bOnGround,
		bool bSkidding, bool bHasClip);
};

/**
 * Run skid in three sections (owner 2026-11-18, decision 2A, brief section 3): ONE clip (the owner's SS_180_Turn_Root180, untouched) cut in
 * entry / hold / push. The entry and the push play at a fixed length (the kit's EnterFrames / ExitFrames); only the HOLD stretches, driven by
 * the speed left (1 - speed / entry speed, ease-out), so the skid lasts what the simulation says at every run stage without slowing the cut in
 * or the push off. Section starts are fractions of the clip (keys 7 and 24 of 26 on Root180).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_SkidSectionSettings
{
	GENERATED_BODY()

	/** Off = the skid layer's own clip and stretch (the behaviour before) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid")
	TSoftObjectPtr<UAnimSequenceBase> Clip =
		TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn_Root180.SS_180_Turn_Root180")));

	/** Where the hold starts and the push starts, as fractions of the clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoldStartFraction = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PushStartFraction = 0.92f;

	/** Ease-out of the hold (1 = linear on the speed left, 2 = quadratic) and ease-in of the push */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float HoldEaseOut = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skid", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float PushEaseIn = 1.5f;

	/** The clip time of a skid frame. Frame from 0; Speed / EntrySpeed: the slide speed now and at the first frame */
	float ClipTime(EGSTN_RunSkidPhase Phase, int32 Frame, int32 RestFrame, int32 EnterFrames, int32 ExitFrames, float Speed, float EntrySpeed, float Length) const;
};

/** How the character stops when the stick is let go during a run (no reverse input: that is the skid) */
UENUM(BlueprintType)
enum class EGSTN_RunStopMode : uint8
{
	/** No stop state: GASP's motion matching picks its own stop (the behaviour before) */
	Off,
	/** The dedicated stop clip (Smash-style braking pose), timed on the sim's braking */
	Clip,
	/** A plain cross-fade from the run into the idle pose */
	Blend
};

/**
 * RUN STOP (owner 2026-11-18, brief run-start-stop-skid section 2; reference Ganondorf's run stop): stick let go with no reverse input. A clip
 * of its own, played by the RunStop state of the main ABP (raw clip, never through motion matching). The sim brakes much faster than any stop
 * clip, so the braking part is timed on the SPEED (clip time where the clip's root speed / its run speed = sim speed / speed at release): the
 * front foot plants when the body actually stops, at every run stage. After the stop the settle (torso rises, arms close) plays at SettleRate.
 * Two clips, one per planted foot: the foot that is swinging at release plants in front.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_RunStopSettings
{
	GENERATED_BODY()

	/** Console override: gstn.RunStop.Mode (-1 = this value, 0 off, 1 clip, 2 blend) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop")
	// Off until a stop clip passes the review (run-stop-1: the GASP Neutral stop reads as an upright jog-to-stop, not Ganondorf's brake)
	EGSTN_RunStopMode Mode = EGSTN_RunStopMode::Off;

	/** The stop that plants the LEFT foot in front, and the one that plants the right */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop")
	TSoftObjectPtr<UAnimSequenceBase> ClipLeftFoot =
		TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Stop_F_Lfoot.M_Neutral_Run_Stop_F_Lfoot")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop")
	TSoftObjectPtr<UAnimSequenceBase> ClipRightFoot =
		TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Stop_F_Rfoot.M_Neutral_Run_Stop_F_Rfoot")));

	/** The pose the Blend mode cross-fades into (and the idle the clip settles into) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop")
	TSoftObjectPtr<UAnimSequenceBase> IdleClip =
		TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop.M_Neutral_Stand_Idle_Loop")));

	/** Only a run stops this way: the speed when the stick was let go */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinEntrySpeed = 300.f;

	/** Stick below this = let go; it must stay there ConfirmFrames before the stop starts (a flick back passes through neutral: skid) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StickDeadzone = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0", ClampMax = "10"))
	int32 ConfirmFrames = 2;

	/** Below this the body counts as stopped: the settle starts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float StoppedSpeed = 10.f;

	/** Play rate of the settle after the stop (clip seconds per second) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float SettleRate = 1.5f;

	/** Clip seconds of settle after the clip's own stop, then the state hands over to idle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SettleClipSeconds = 1.3f;

	/** Safety: a body that keeps sliding without the stick (pushed, on ice, a frozen proxy) leaves the brake after this long and settles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float MaxBrakeSeconds = 0.8f;

	/** Blend mode: how long the state stays on (its transition's cross-fade does the interpolation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Run Stop", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float BlendSeconds = 0.35f;
};

/** A stop clip's root speed over time (60 Hz), measured once when the clip loads. Pure: the tests build it from numbers */
struct GSTNCOMBATNP_API FGSTN_RunStopTrack
{
	float SampleRate = 60.f;
	/** Horizontal root speed, cm/s, one per sample */
	TArray<float> Speed;
	float RunSpeed = 0.f;
	/** Sample indices: last one at run speed before the brake, first one at rest */
	int32 BrakeStart = 0;
	int32 StopIndex = 0;

	bool IsValid() const { return RunSpeed > 1.f && StopIndex > BrakeStart; }
	float BrakeStartTime() const { return BrakeStart / SampleRate; }
	float StopTime() const { return StopIndex / SampleRate; }

	/** Builds the track from root positions sampled at SampleRate */
	void Build(const TArray<FVector>& RootPositions, float InSampleRate);

	/** The braking clip time whose root speed share matches SpeedFraction (1 = brake start, 0 = stop) */
	float TimeForSpeedFraction(float SpeedFraction) const;
};

/** The run stop between frames, game thread. Pure but for the inputs: the tests drive it */
struct GSTNCOMBATNP_API FGSTN_RunStopState
{
	EGSTN_RunStopMode Active = EGSTN_RunStopMode::Off;
	bool bLeftFoot = true;
	float ClipTime = 0.f;
	float EntrySpeed = 0.f;
	float Elapsed = 0.f;
	/** Speed while the stick was last held, and frames since it was let go (-1 = held) */
	float HeldSpeed = 0.f;
	int32 FramesReleased = -1;

	bool IsClip() const { return Active == EGSTN_RunStopMode::Clip; }
	bool IsBlend() const { return Active == EGSTN_RunStopMode::Blend; }

	/**
	 * One presented frame. bBlocked: anything that is not a plain grounded run (air, crouch, skid, slide). bLeftFootSwinging: which foot plants
	 * if a stop starts now. Track: the chosen foot's track (Tracks[0] left, [1] right), null = no clip loaded (Clip mode then does nothing).
	 */
	void Step(const FGSTN_RunStopSettings& Settings, EGSTN_RunStopMode Mode, float DeltaSeconds, float Speed, float Stick, bool bBlocked,
		bool bLeftFootSwinging, const FGSTN_RunStopTrack* LeftTrack, const FGSTN_RunStopTrack* RightTrack);
};

/**
 * The LOCOMOTION ANIMATION PROFILE (owner 2026-11-18: locomotion animation depends on the character, body, weight and state, not only on the
 * kit). It is an anim set in slot Anim.Set.Locomotion, so it is chosen like every other anim set (UGSTN_AnimSetComponent): the character's
 * collection by default, a kit, weapon or skin collection above it, variants by cosmetic tags (Anim.Trait.Build.Heavy...). Presentation only;
 * the kit's FGSTN_LocomotionSimTuning keeps the gameplay numbers.
 *
 * Today it holds the lean. Planned sections (docs/notes/additive-lean-1.md): the GASP locomotion set (chooser table / pose search databases),
 * the skid and run start presentation values (their clips already live in the Anim.Set.Skid / Anim.Set.RunStart sets).
 */
UCLASS(BlueprintType)
class GSTNCOMBATNP_API UGSTN_LocomotionAnimSet : public UGSTN_AnimSetDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FGSTN_LocomotionLeanSettings Lean;

	/**
	 * The main locomotion anim blueprint (owner 2026-11-18: our copy of GASP's Mover ABP and raw chooser, ABP_GSTN_Locomotion, where the skid
	 * row and state live). Empty = the mesh keeps its own class (GASP's original): going back is clearing this field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	TSoftClassPtr<UAnimInstance> MainAnimClass;

	/** The skid plays in MainAnimClass's Skid state (GetSkidClipTime), not in the skid override layer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	bool bSkidInMainGraph = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FGSTN_RunStartScrambleSettings RunStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FGSTN_SkidSectionSettings Skid;

	/** Stick let go during a run: the RunStop state of MainAnimClass */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FGSTN_RunStopSettings RunStop;
};

/** The lean's state between frames, game thread. Pure but for the inputs: the tests drive it */
struct GSTNCOMBATNP_API FGSTN_LocomotionLeanSolver
{
	/** Current lean: horizontal world vector, length = degrees, direction = where the upper body goes */
	FVector Lean = FVector::ZeroVector;
	FVector LeanRate = FVector::ZeroVector;
	FVector FilteredAccel = FVector::ZeroVector;
	FVector PrevVelocity = FVector::ZeroVector;
	bool bHasPrev = false;

	/**
	 * One presented frame. Velocity: presented world velocity; Forward: the facing (used while too slow for a heading); StateScale: run stage
	 * x air / attack / slide scale. Returns Lean
	 */
	FVector Step(const FGSTN_LocomotionLeanSettings& Settings, const FVector& Velocity, const FVector& Forward, float DeltaSeconds, float StateScale);

	/** The target lean of an acceleration at a speed (no smoothing): the tests check the directions on it */
	static FVector TargetLean(const FGSTN_LocomotionLeanSettings& Settings, const FVector& HorizontalVelocity, const FVector& HorizontalAccel,
		const FVector& Forward, float StateScale);

	void Reset() { *this = FGSTN_LocomotionLeanSolver(); }
};
