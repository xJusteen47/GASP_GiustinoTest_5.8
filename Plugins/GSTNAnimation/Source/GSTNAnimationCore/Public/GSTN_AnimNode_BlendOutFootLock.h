// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "GSTN_AnimNode_BlendOutFootLock.generated.h"

/** What one foot is doing (runtime state of the blend-out foot lock) */
UENUM()
enum class EGSTN_FootLockPhase : uint8
{
	Free,      // the pose passes through untouched
	Locked,    // the foot holds the world transform it had when the montage started blending out
	Stepping,  // a short step (lift + place) from the held spot to where the game pose wants the foot
	Releasing, // a short blend back to the game pose (foot already close, or the capsule started moving)
	Smoothing  // pop guard: the foot jumped in one frame (a foot pin letting go, a hard blend); the jump fades out instead of showing
};

/** One leg of the lock: the foot bone, its two parents are the IK chain (calf, thigh) */
USTRUCT()
struct GSTNANIMATIONCORE_API FGSTN_FootLockLeg
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Foot;

	// Runtime
	FCompactPoseBoneIndex FootIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex CalfIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex ThighIndex = FCompactPoseBoneIndex(INDEX_NONE);
	EGSTN_FootLockPhase Phase = EGSTN_FootLockPhase::Free;
	FTransform Held = FTransform::Identity;   // world transform the foot holds
	FVector LastInputCS = FVector::ZeroVector;
	bool bHasLastInput = false;
	int32 SettledFrames = 0;
	float PhaseTime = 0.f;
	float StepDelay = 0.f;
	float Alpha = 0.f;

	// Pop guard (every frame, whatever the phase): the input foot's world track and the foot this node showed last frame
	FVector LastInWorld = FVector::ZeroVector;
	FVector LastInVelocity = FVector::ZeroVector;
	FVector LastOutWorld = FVector::ZeroVector;
	FQuat LastOutRotation = FQuat::Identity;
	bool bHasTrack = false;
	FVector PopOffset = FVector::ZeroVector;     // world offset from the input foot to the shown foot when the jump happened
	FQuat PopRotationOffset = FQuat::Identity;   // shown rotation * inverse(input rotation) at the jump

	bool IsValid() const { return FootIndex != INDEX_NONE && CalfIndex != INDEX_NONE && ThighIndex != INDEX_NONE; }
};

/**
 * Keeps the feet planted while a montage blends back to the game pose, then puts each foot down where the game pose wants it with a short
 * step instead of a slide. It reads the request from UGSTN_CharacterAnimInstance (bFootLockRequest / bFootLockInstantRelease), so it needs no
 * pin. While no foot is locked the pose passes through untouched (GASP locomotion foot placement unchanged), except where a foot pops: the
 * pop guard (PopDistance) fades a one-frame foot jump out over PopBlendTime (a.GSTN.FootPopGuard 0 turns only the guard off).
 * Place it in the main AnimGraph right after the Procedural_Feet layer. Console: a.GSTN.BlendOutFootLock 0 turns it off (pass through).
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_BlendOutFootLock : public FAnimNode_Base
{
	GENERATED_BODY()

	FGSTN_AnimNode_BlendOutFootLock();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_FootLockLeg LeftLeg;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FGSTN_FootLockLeg RightLeg;

	/** A held foot farther than this (cm) from where the game pose wants it ends with a step; closer, it blends there */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float StepThreshold = 4.f;

	/** Seconds of one step */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float StepTime = 0.15f;

	/** Peak lift of a step (cm) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float StepLift = 4.f;

	/** The second foot starts its step when the first is this far into its own (0..1) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SecondStepStart = 0.6f;

	/** Seconds of the blend back when no step is needed or the capsule starts moving */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float ReleaseTime = 0.1f;

	/** The game pose has settled when the input foot moves slower than this (cm/s, component space)... GASP's idle settle after a montage
	 *  moves the feet faster than 12 cm/s until ~30 frames after the blend-out starts, then holds (night-footlock captures) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SettleSpeed = 10.f;

	/** ...for this many frames in a row, with the montage fully blended out */
	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 SettleFrames = 4;

	/** A held foot this far (cm) from the game pose steps at once (safety) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float MaxHoldDistance = 40.f;

	/** The component jumping this far (cm) in a frame frees the feet (teleport) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float TeleportDistance = 50.f;

	/**
	 * Pop guard, for any clip: a foot that moves in one frame this much (cm) farther than its last speed explains has popped: a foot pin
	 * letting go after the body turned under it, a hard blend between two poses. The jump is kept as an offset that fades out over
	 * PopBlendTime, so the foot travels there instead of teleporting. A stop never counts (a run foot pinned at once moves less than its
	 * speed); a swinging foot moves about as far as its speed says. 0 turns the guard off.
	 * Skid exit, anim-setup-2: the GASP foot pin let go of the right foot 15 cm off at clip time 0.333 of SS_180_Turn_Root180.
	 */
	UPROPERTY(EditAnywhere, Category = "Pop Guard", meta = (ClampMin = "0", ForceUnits = "cm"))
	float PopDistance = 8.f;

	/** A one-frame foot jump wider than this (cm) is a real change of pose, not a pop: the guard leaves it alone (offsets that big
	 *  hid a real swing at the skid exit and released it as a whip) */
	UPROPERTY(EditAnywhere, Category = "Pop Guard", meta = (ClampMin = "0", ForceUnits = "cm"))
	float PopMaxDistance = 20.f;

	/** A pop is a STILL foot jumping (a pin letting go): the guard only fires when the foot moved slower than this (cm/s) the frame
	 *  before. A swinging or accelerating foot is real motion (skid entry left foot) */
	UPROPERTY(EditAnywhere, Category = "Pop Guard", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float PopStillSpeed = 60.f;

	/** Seconds over which a pop's offset fades to the game pose, (1 - t)^2: the foot starts moving on the first frame */
	UPROPERTY(EditAnywhere, Category = "Pop Guard", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float PopBlendTime = 0.15f;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
	void FreeAll();
	void BeginRelease(const FTransform InputWorld[2], bool bAllowStep);

	float DeltaTime = 0.f;
	bool bRequest = false;
	bool bPrevRequest = false;
	bool bInstantRelease = false;
	bool bMoving = false;
	bool bPopGuard = false;
	float MontageWeight = 0.f;
	FVector LastComponentLocation = FVector::ZeroVector;
	bool bHasLastComponent = false;
};
