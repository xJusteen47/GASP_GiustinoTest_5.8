// Combat presentation (GAS-NPP fork): what the run skid and the attack slides show (docs/notes/corsa-mario-piano.md, P2). Never read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_AdditiveReactionSource.h"
#include "GSTN_PoseDriverSource.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "Presentation/GSTN_LocomotionLean.h"
#include "GSTN_RunPresentationComponent.generated.h"

class UMoverComponent;
class UAnimSequenceBase;
class UAnimInstance;

/** Which sliding move the feet slide in */
UENUM(BlueprintType)
enum class EGSTN_FootSlideSource : uint8
{
	None,
	RunSkid,
	/** A ground attack's momentum slide (FGSTN_LayeredMove_MomentumSlide) */
	AttackSlide
};

/**
 * The foot slide hook (plan: "solo l'hook, niente VFX"): the feet slide on the ground in a skid or an attack slide. Decals, dust and sound
 * hang on it from Blueprint (OnFootSlideChanged + GetFootSlide each frame); the anim skid layer (L3) will read the phase.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_FootSlideState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	EGSTN_FootSlideSource Source = EGSTN_FootSlideSource::None;

	/** The skid's phase (None for an attack slide) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	EGSTN_RunSkidPhase SkidPhase = EGSTN_RunSkidPhase::None;

	/** Sliding speed on the ground, cm/s (0 in the skid's exit) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	float Speed = 0.f;

	/** Unit sliding direction, world */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	FVector Direction = FVector::ZeroVector;

	/** Live frames since the slide started */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	int32 Frame = 0;

	/** Skid only: frames from the start to rest, and the distance left to the stop (the anim layer's distance matching) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	int32 RestFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	float DistanceToStop = 0.f;

	/** Skid only (skid-physics-1): the frames of this skid's push (its Exit phase), from the simulation; 0 = the kit's ExitFrames */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	int32 ExitFrames = 0;

	/** Skid only: the capsule follows the skid clip (ClipRoot motion), so the clip may play at its own speed (gstn.Skid.PlayAtClipSpeed).
	 *  False = the clip follows the simulation's phases (the animation follows the physics) */
	UPROPERTY(BlueprintReadOnly, Category = "Foot Slide")
	bool bClipTimed = false;

	bool IsSliding() const { return Source != EGSTN_FootSlideSource::None; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnFootSlideChanged, bool, bSliding, const FGSTN_FootSlideState&, State);

/**
 * Reads the presented Mover state (game thread, after the movement of the frame) and turns the sliding moves into presentation:
 *   - the foot slide hook above;
 *   - the PLACEHOLDER skid pose until the real clips come with the anim layer plan (anim-layer-skid-wallslide-piano L0/L3): the torso leans
 *     back against the slide through the directional additive node already in the main graph (IGSTN_AdditiveReactionSource), by the
 *     kit's FGSTN_RunSkidTuning::PlaceholderLeanDeg (layer 0 of the request);
 *   - the additive LOCOMOTION LEAN (layer 1): the torso leans into the presented acceleration, values in the character's locomotion animation
 *     profile (UGSTN_LocomotionAnimSet, slot Anim.Set.Locomotion; built-in defaults while none is applied). Console: a.GSTN.LocomotionLean 0.
 * No Blueprint wiring: UGSTN_CombatSimComponent creates this component at BeginPlay.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class GSTNCOMBATNP_API UGSTN_RunPresentationComponent : public UActorComponent, public IGSTN_AdditiveReactionSource, public IGSTN_PoseDriverSource
{
	GENERATED_BODY()

public:
	UGSTN_RunPresentationComponent();

	/** Adds the component to Owner once (game thread, outside the simulation) */
	static UGSTN_RunPresentationComponent* FindOrAdd(AActor* Owner);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The foot slide of the presented frame */
	UFUNCTION(BlueprintPure, Category = "Combat|Run")
	const FGSTN_FootSlideState& GetFootSlide() const { return FootSlide; }

	/** A slide started (true) or ended (false) */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Run")
	FGSTN_OnFootSlideChanged OnFootSlideChanged;

	/** Off = no placeholder lean (the real skid layer replaces it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Run")
	bool bPlaceholderSkidLean = true;

	virtual void GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const override;

	/** IGSTN_PoseDriverSource: the run stop clip owns the pose (RunStop.IsClip); the feet are held through its hand-off to the idle */
	virtual bool IsDrivingPose(float& OutHandOffSeconds) const override;

	/** The locomotion lean of the presented frame: horizontal world vector, length = degrees (debug, cosmetics) */
	UFUNCTION(BlueprintPure, Category = "Combat|Run")
	FVector GetLocomotionLean() const { return LeanSolver.Lean; }

	/** The lean values in use: the applied locomotion profile's, else the built-in defaults */
	const FGSTN_LocomotionLeanSettings& GetLeanSettings() const;

	/** The applied locomotion profile's run start and skid values, else the built-in defaults */
	const FGSTN_RunStartScrambleSettings& GetScrambleSettings() const;
	const FGSTN_SkidSectionSettings& GetSkidSectionSettings() const;

	/** The run start scramble of the presented frame and its loaded run cycle (null = no scramble) */
	const FGSTN_RunStartScrambleState& GetScramble() const { return Scramble; }
	UAnimSequenceBase* GetScrambleClip() const { return ScrambleClip; }

	/**
	 * For the Skid state of the main locomotion ABP (ABP_GSTN_Locomotion, owner 2026-11-18): true while the run skid is on (rule in) and its
	 * three-section clip time (Sequence Evaluator explicit time, clip SS_180_Turn_Root180). With the profile's bSkidInMainGraph the skid
	 * override layer stays off, so the two never play together.
	 * Routing fix 2026-11-19: the main ABP's state machine outputs no pose, so its states now play their clip through the Blend Stack
	 * (UGSTN_LocomotionStateLibrary, chooser CHT_GSTN_OwnStates) at the clip's own rate; the clip-time getters here no longer drive the pose.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	bool IsSkidding() const { return SkidPlayTime >= 0.f || FootSlide.Source == EGSTN_FootSlideSource::RunSkid; }

	/** gstn.Skid.PlayAtClipSpeed on AND the last skid moved the capsule on its clip (ClipRoot): only then the clip plays at its own speed.
	 *  A Physics skid always stretches the clip on the simulation's phases (skid-physics-1: the animation follows the physics) */
	bool IsSkidClipSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	float GetSkidClipTime() const;

	/**
	 * The same two values read from an Animation Blueprint without a component reference: the owning actor's run presentation component,
	 * or false / 0 when it has none. AnimInstance defaults to self, so a transition rule or a Sequence Evaluator pin calls them directly.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerSkidding(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerSkidClipTime(const UAnimInstance* AnimInstance);

	/** Seconds since the current skid began, -1 when not skidding: the Skid state leaves early (its turn is done) and must not re-enter */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerSkidElapsed(const UAnimInstance* AnimInstance);

	/** Horizontal speed of the owner (cm/s), cached on the game thread each tick: the Skid state hands off to the run loop only when the
	 *  stick still drives the character the new way (locomotion-finish-3; with the stick released the skid ends facing the new way) */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerGroundSpeed(const UAnimInstance* AnimInstance);

	/** Move input along the owner's horizontal velocity, -1..1 (stick size times the cosine; the stick size when still), cached on the
	 *  game thread each tick: Skid Exit Run leaves at once when the stick is released or turned away (skid-finish-4) */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerStickAlong(const UAnimInstance* AnimInstance);

	/**
	 * For the RunStop states of the main locomotion ABP (owner 2026-11-18, stick let go during a run with no reverse input; FGSTN_RunStopSettings):
	 * IsRunStopping = the stop clip plays (state "Run Stop", Sequence Evaluator: clip GetRunStopClip, explicit time GetRunStopClipTime);
	 * IsRunStopBlending = the plain cross-fade into idle (state "Run Stop Blend", the profile's idle clip). Never both; never during a skid.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	bool IsRunStopping() const { return RunStop.IsClip(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	bool IsRunStopBlending() const { return RunStop.IsBlend(); }

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	UAnimSequenceBase* GetRunStopClip() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	float GetRunStopClipTime() const { return RunStop.ClipTime; }

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerRunStopping(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerRunStopBlending(const UAnimInstance* AnimInstance);

	/** The stop plants the left foot in front (state "Run Stop": Blend Poses by bool between the left-foot and right-foot clips) */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerRunStopLeftFoot(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static UAnimSequenceBase* GetAnimOwnerRunStopClip(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerRunStopClipTime(const UAnimInstance* AnimInstance);

	/** Fast crouch (brief crouch-veloce-1): the phase the main ABP's crouch states read, and the clip time of Enter / Exit */
	UFUNCTION(BlueprintPure, Category = "Combat|Crouch", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerFastCrouchEntering(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Crouch", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerFastCrouchIdle(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Crouch", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerFastCrouchExiting(const UAnimInstance* AnimInstance);

	/** Any phase but Off (the states' way back to the GASP graph) */
	UFUNCTION(BlueprintPure, Category = "Combat|Crouch", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerFastCrouching(const UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintPure, Category = "Combat|Crouch", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerFastCrouchTime(const UAnimInstance* AnimInstance);

	const FGSTN_FastCrouchState& GetFastCrouch() const { return FastCrouch; }

	const FGSTN_RunStopSettings& GetRunStopSettings() const;
	const FGSTN_RunStopState& GetRunStop() const { return RunStop; }

	/**
	 * Frantic run start (owner 2026-11-18, choice B): seconds since the run start stamp began on this push of the stick (the sim's
	 * FGSTN_RunStageState::StampFrames), -1 when this push had no stamp or the stick rests. It keeps counting after the stamp, so the
	 * start clip (stamp + frantic steps) plays to its end on a Sequence Evaluator with this as explicit time; the state leaves on the clip's
	 * end or on -1. IsRunStartStamp: the almost-in-place part is live (the sim holds the speed down).
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	float GetRunStartTime() const { return RunStartTime; }

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	bool IsRunStartStamp() const { return bRunStartStamp; }

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerRunStartTime(const UAnimInstance* AnimInstance);

	/**
	 * 1 once the start clip should hand off to the run (stamp over, then the clip's first foot plant or RunStart.HandOffMaxAfterStamp;
	 * FGSTN_RunStartScrambleSettings), 0 otherwise. Latched until the run start time goes back to -1. Exit rule of the "Run Start" state.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerRunStartHandOff(const UAnimInstance* AnimInstance);

	bool IsRunStartHandOff() const { return bRunStartHandOff; }

	/**
	 * Boost dash (L3, owner 2026-11-19, Smash's dash): seconds since the boost on this push of the stick, -1 when none. The run start time
	 * above goes to -1 on the boost, so the frantic start clip leaves and a dash clip (its own state) can take over with this time.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe))
	float GetBoostDashTime() const { return BoostDashTime; }

	UFUNCTION(BlueprintPure, Category = "Combat|Run", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static float GetAnimOwnerBoostDashTime(const UAnimInstance* AnimInstance);

	/**
	 * Jump squat (owner 2026-11-17, 1A: 3 frames on the ground before the take-off; UGSTN_CombatSettings::JumpSquatFrames): true while the
	 * combat sim's jump state counts the squat down (FGSTN_JumpState::SquatFramesLeft > 0). Read straight from the simulated jump state, not
	 * cached in this component's tick (TG_PostPhysics, one frame late): the squat lasts 3 frames, a late frame would lose a third of it.
	 * Entry/exit rule of the "Jump Squat" state of the main ABP (Tools/GAS/jump_squat/jump_squat_states.py)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Jump", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance", HidePin = "AnimInstance"))
	static bool IsAnimOwnerJumpSquatting(const UAnimInstance* AnimInstance);

	/** The profile moves the skid into the main ABP (the skid layer then never takes the pose) */
	bool IsSkidInMainGraph() const;

	/** The three-section skid clip (null = sections off or not loaded: the skid layer keeps its own clip) and the skid's entry speed */
	UAnimSequenceBase* GetSkidSectionClip() const { return SkidSectionClip; }
	float GetSkidEntrySpeed() const { return SkidEntrySpeed; }
	int32 GetSkidEnterFrames() const { return EnterFrames; }
	int32 GetSkidExitFrames() const { return ExitFrames; }

	/** The foot slide of a Mover sync state (pure but for the kit lookup; the tests call it) */
	static FGSTN_FootSlideState ReadFootSlide(const struct FMoverSyncState& SyncState, float StepSeconds);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMoverComponent> Mover;

	void TickLean(float DeltaTime);
	void TickScramble(float DeltaTime);
	void ResolveClips();
	/** Puts the profile's main anim class on the character's mesh (the anim set component relinks the layers on OnAnimInitialized) */
	void ApplyMainAnimClass();
	FSoftObjectPath ResolvedMainAnimClass;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> ScrambleClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> SkidSectionClip;

	FGSTN_RunStartScrambleState Scramble;
	FGSTN_RunStartScrambleSettings DefaultScrambleSettings;
	FGSTN_SkidSectionSettings DefaultSkidSettings;
	/** The soft paths last resolved (a profile change reloads) */
	FSoftObjectPath ResolvedScramblePath;
	FSoftObjectPath ResolvedSkidPath;
	float SkidEntrySpeed = 0.f;
	/** Cached on the game thread each tick (the anim worker reads them) */
	float RunStartTime = -1.f;
	bool bRunStartStamp = false;

	/** Hand-off of the start clip (UpdateRunStartHandOff): latched flag, run start time when the stamp ended, last contact curve states */
	void UpdateRunStartHandOff();
	bool bRunStartHandOff = false;
	float RunStartStampEndTime = -1.f;
	bool bRunStartContactL = false;
	bool bRunStartContactR = false;
	float BoostDashTime = -1.f;
	/**
	 * Clip speed (cvar gstn.Skid.PlayAtClipSpeed, owner choice A): seconds since the skid started, < 0 = no skid shown. Runs on past the
	 * sim's skid while the clip has frames left and the character stays on the ground with nothing else sliding (the restart is in the clip)
	 */
	float SkidPlayTime = -1.f;
	void TickSkidPlayTime(float DeltaTime);
	/** FGSTN_FootSlideState::bClipTimed of the last live skid frame, kept for the clip's tail after the sim's skid */
	bool bLastSkidClipTimed = false;
	FVector ScrambleDirection = FVector::ForwardVector;

	/** Run stop: the two clips (left foot, right foot in front), their root speed tracks, the idle of the blend mode, and the state */
	void TickRunStop(float DeltaTime);
	void TickFastCrouch(float DeltaTime);
	/** The mesh's left foot is the one in the air (higher than the right): it is the one that plants in front */
	bool IsLeftFootSwinging() const;
	/** The stick, 0..1 (a sim proxy without inputs counts as a full push) */
	float ReadStick() const;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> RunStopClips[2];
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> RunStopIdleClip;
	FSoftObjectPath ResolvedRunStopPaths[3];
	FGSTN_RunStopTrack RunStopTracks[2];
	FGSTN_RunStopState RunStop;
	/** GetAnimOwnerSkidElapsed */
	float SkidElapsed = -1.f;

	/** GetAnimOwnerGroundSpeed */
	float GroundSpeed = 0.f;
	/** GetAnimOwnerStickAlong */
	float StickAlong = 1.f;
	FGSTN_RunStopSettings DefaultRunStopSettings;
	FGSTN_FastCrouchState FastCrouch;
	FGSTN_FastCrouchSettings DefaultFastCrouchSettings;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> FastCrouchClips[2];
	FSoftObjectPath ResolvedFastCrouchPaths[2];

	FGSTN_FootSlideState FootSlide;
	FGSTN_LocomotionLeanSolver LeanSolver;
	FGSTN_LocomotionLeanSettings DefaultLeanSettings;
	TWeakObjectPtr<const UGSTN_LocomotionAnimSet> LeanProfile;
	float LeanDeg = 0.f;
	int32 EnterFrames = 0;
	int32 ExitFrames = 0;
};
