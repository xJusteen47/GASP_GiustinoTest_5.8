// Pure camera types (camera.md §2, §5, §10). No Engine, no world: every value is plain data the maths and the tests build by hand.
// Everything is relative to Up (camera.md §7.2): headings are degrees about Up from the frame's reference forward, pitch is the
// elevation above the plane normal to Up, so a view never contains a world Z assumption.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CameraTypes.generated.h"

/** How a blend weight follows its progress (camera.md §2.2). All functions are monotonic from 0 to 1 */
UENUM(BlueprintType)
enum class EGSTN_CameraBlendFunction : uint8
{
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut
};

/** Which policy resolves a blocker between the pivot and the camera (camera.md §7.1) */
UENUM(BlueprintType)
enum class EGSTN_CameraOcclusionPolicy : uint8
{
	/** The camera moves in front of the blocker */
	PullIn,
	/** The blocker fades out; the camera does not move */
	Fade,
	/** Fade what is fadeable, pull in only for non-fadeable blockers close to the camera */
	FadeThenPullIn
};

/** What to do with one blocker */
UENUM(BlueprintType)
enum class EGSTN_CameraOcclusionAction : uint8
{
	None,
	Fade,
	PullIn
};

/** Why the camera turned its reference direction (camera.md §5) */
UENUM(BlueprintType)
enum class EGSTN_CameraFrameChangeReason : uint8
{
	SectionEntered,
	SectionExited,
	ModeChanged,
	Respawn,
	Other
};

/** The camera frame: Up and a reference forward perpendicular to it. Headings are measured from RefForward about Up */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraFrame
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Frame")
	FVector Up = FVector::UpVector;

	/** Unit, perpendicular to Up. Parallel transported when Up changes so the view of the world turns continuously */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Frame")
	FVector RefForward = FVector::ForwardVector;
};

/**
 * One camera view in the gravity frame: the pivot, the heading and pitch of the look direction, the arm and the offsets.
 * Blended field by field (camera.md §2.2 step 4); the world transform is derived by FGSTN_CameraMath::ComputePose.
 */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View")
	FVector Pivot = FVector::ZeroVector;

	/** Degrees about Up from the frame's RefForward, the horizontal direction the camera looks along */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View", meta = (ForceUnits = "deg"))
	float HeadingDeg = 0.f;

	/** Elevation of the look direction above the plane normal to Up: negative looks down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View", meta = (ForceUnits = "deg"))
	float PitchDeg = -25.f;

	/** Distance from the pivot back along the look direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ArmCm = 650.f;

	/** Pivot offset to the right of the look direction (over-the-shoulder) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View", meta = (ForceUnits = "cm"))
	float SideOffsetCm = 0.f;

	/** Pivot offset along Up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View", meta = (ForceUnits = "cm"))
	float UpOffsetCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|View", meta = (ClampMin = "5", ClampMax = "170", ForceUnits = "deg"))
	float FovDeg = 65.f;
};

/** The world pose of a view */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraPose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pose")
	FVector Location = FVector::ZeroVector;

	/** Unit look direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pose")
	FVector Forward = FVector::ForwardVector;

	/** Unit, perpendicular to Forward, on the Up side: the roll that keeps the camera's up on gravity-up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pose")
	FVector Up = FVector::UpVector;
};

/** Tuning of one blend (camera.md §10 Camera|Blend) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraBlendTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Blend", meta = (ClampMin = "0", ForceUnits = "s"))
	float BlendSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Blend")
	EGSTN_CameraBlendFunction Function = EGSTN_CameraBlendFunction::EaseInOut;

	/** Exponent of the Ease functions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Blend", meta = (ClampMin = "1", ClampMax = "8"))
	float Exponent = 2.f;
};

/** One entry of the mode stack. Key identifies the mode definition; Progress (0..1) is the raw blend progress, the weight is its function */
USTRUCT()
struct GSTNCAMERACORE_API FGSTN_CameraStackEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Key = INDEX_NONE;

	UPROPERTY()
	float Progress = 0.f;

	UPROPERTY()
	FGSTN_CameraBlendTuning Blend;
};

/**
 * The mode stack as pure data (camera.md §2.2 steps 2 and 4). The last entry is the top. A request for a mode already in the stack
 * moves it to the top KEEPING its progress, so a reversed request reverses the blend from where it is, not from zero.
 */
USTRUCT()
struct GSTNCAMERACORE_API FGSTN_CameraModeStack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGSTN_CameraStackEntry> Entries;

	bool IsEmpty() const { return Entries.IsEmpty(); }
	int32 Num() const { return Entries.Num(); }
	int32 TopKey() const { return Entries.IsEmpty() ? INDEX_NONE : Entries.Last().Key; }
	int32 IndexOf(int32 Key) const;

	/** Push, or promote to the top. A first entry starts complete. Entries beyond MaxDepth drop from the bottom; their keys go to OutRemoved */
	void Request(int32 Key, const FGSTN_CameraBlendTuning& Blend, int32 MaxDepth, TArray<int32>* OutRemoved = nullptr);

	/** Advance the top entry's progress; entries below a complete entry drop (their keys go to OutRemoved) */
	void Advance(float DeltaSeconds, TArray<int32>* OutRemoved = nullptr);

	/** Blend weight of an entry: the bottom one is always 1 */
	float WeightOf(int32 Index) const;

	/** Drop everything but the top entry and complete it (a cut) */
	void Cut(TArray<int32>* OutRemoved = nullptr);
};

/** Orbit state the player controls, in the frame (camera.md §4.3). Lives per local player */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraOrbitState
{
	GENERATED_BODY()

	/** Absolute heading about Up from the frame's RefForward */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ForceUnits = "deg"))
	float YawDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ForceUnits = "deg"))
	float PitchDeg = -25.f;

	/** Multiplies the arm length (zoom), 1 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit", meta = (ClampMin = "0.1"))
	float DistanceScale = 1.f;
};

/** Limits of a manual orbit around a desired angle (camera.md §4.1 manual control group) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraOrbitLimits
{
	GENERATED_BODY()

	/** Maximum yaw offset either side of the desired heading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float YawLimitDeg = 45.f;

	/** Pitch offsets from the desired pitch (negative below, positive above) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ForceUnits = "deg"))
	float PitchMinOffsetDeg = -15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ForceUnits = "deg"))
	float PitchMaxOffsetDeg = 15.f;

	/** 1 = no zoom; above 1 allows DistanceScale between 1/Limit and Limit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Section", meta = (ClampMin = "1"))
	float DistanceScaleLimit = 1.f;
};

/** A section that contains the view target, reduced to the three numbers that order sections (camera.md §4.2) */
struct GSTNCAMERACORE_API FGSTN_CameraSectionCandidate
{
	int32 Id = INDEX_NONE;
	int32 Priority = 0;
	/** Seconds on the camera clock when the target entered it: larger = entered later */
	double EnterTime = 0.0;
	/** Volume measure: smaller wins the last tie */
	double VolumeSize = 0.0;
};

/** One rule of the rule table, reduced to what the debounce needs (camera.md §3.1, §3.4). The tag query is evaluated by the runtime */
struct GSTNCAMERACORE_API FGSTN_CameraRuleEval
{
	int32 Priority = 0;
	bool bMatches = false;
	float EnterDelaySeconds = 0.f;
	float ExitDelaySeconds = 0.15f;
};

/** Debounce state of the rule choice */
USTRUCT()
struct GSTNCAMERACORE_API FGSTN_CameraRuleChoiceState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 CurrentRule = INDEX_NONE;

	UPROPERTY()
	int32 PendingRule = INDEX_NONE;

	UPROPERTY()
	float PendingSeconds = 0.f;

	/** Time the current rule has been chosen */
	UPROPERTY()
	float DwellSeconds = 0.f;
};

/** Stability tuning (camera.md §10 Camera|Stability) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraStabilityTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Stability", meta = (ClampMin = "1", ForceUnits = "deg/s"))
	float UpRateLimitDegPerSecond = 90.f;

	/** Default exit delay of a rule that names none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Stability", meta = (ClampMin = "0", ForceUnits = "s"))
	float ExitDelaySeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Stability", meta = (ClampMin = "0", ForceUnits = "s"))
	float MinDwellSeconds = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Stability", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxStackDepth = 4;
};

/** The far follow camera, Mario 3D World style (camera.md §2.3) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraFollowTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DistanceCm = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float PitchDeg = -25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "5", ClampMax = "170", ForceUnits = "deg"))
	float FovDeg = 65.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "s"))
	float PivotDampingSeconds = 0.25f;

	/** Pitch limits of the manual orbit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float MinPitchDeg = -70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float MaxPitchDeg = 20.f;

	/** The camera never turns with the character unless this is on (then it eases the heading behind the velocity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow")
	bool bAutoAlignToMovement = false;
};

/** The extra rules of the free camera, Astro Bot style (camera.md §2.4); the base numbers come from FGSTN_CameraFollowTuning */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraFreeTuning
{
	GENERATED_BODY()

	/** The pivot stays on the last ground height until the character is this far above it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "cm"))
	float JumpDeadZoneUpCm = 250.f;

	/** The pivot follows down at once when the character drops further than this below the last ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FallFollowCm = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LookAheadCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float LookAheadSeconds = 0.6f;

	/** Speed that gives the full look-ahead */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float LookAheadFullSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow")
	bool bPitchDownWhenFalling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "deg"))
	float FallPitchExtraDeg = 12.f;

	/** The recentre action eases the heading behind the character's facing in this time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float RecenterSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Follow", meta = (ClampMin = "0", ForceUnits = "cm"))
	float SpeedZoomOutCm = 0.f;
};

/** Over the shoulder (camera.md §2.3, §6) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraShoulderTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ArmCm = 220.f;

	/** Offset to the shoulder side and along Up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ShoulderOffsetCm = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float UpOffsetCm = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder")
	bool bRightShoulder = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "5", ClampMax = "170", ForceUnits = "deg"))
	float FovDeg = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "0.05", ClampMax = "2"))
	float AimSensitivityScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float MinPitchDeg = -70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float MaxPitchDeg = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder")
	bool bConvergeAim = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ConvergeMinCm = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shoulder", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ConvergeMaxCm = 6000.f;
};

/** PvP framing, Tekken style (camera.md §2.5) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraDuelTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "1", ForceUnits = "deg/s"))
	float DuelYawRateDegPerSecond = 120.f;

	/** Free space kept around the fighters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DuelMarginCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DuelMinArmCm = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DuelMaxArmCm = 1800.f;

	/** Horizontal separation under this keeps the last heading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "0", ForceUnits = "cm"))
	float DuelMinAxisCm = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "-89", ClampMax = "89", ForceUnits = "deg"))
	float PitchDeg = -12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel", meta = (ClampMin = "5", ClampMax = "170", ForceUnits = "deg"))
	float FovDeg = 55.f;

	/** Local player on the left of its own screen (CAM.8 default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Duel")
	bool bLocalPlayerOnLeft = true;
};

/** Occlusion (camera.md §7.1, §10 Camera|Occlusion) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraOcclusionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion")
	EGSTN_CameraOcclusionPolicy Policy = EGSTN_CameraOcclusionPolicy::FadeThenPullIn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProbeRadiusCm = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion", meta = (ClampMin = "0", ForceUnits = "s"))
	float FadeSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion", meta = (ClampMin = "0", ClampMax = "1"))
	float FadedOpacity = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion", meta = (ClampMin = "0", ForceUnits = "s"))
	float PullInSecondsIn = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion", meta = (ClampMin = "0", ForceUnits = "s"))
	float PullInSecondsOut = 0.6f;

	/** FadeThenPullIn pulls in only for a non-fadeable blocker within this distance of the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Occlusion", meta = (ClampMin = "0", ForceUnits = "cm"))
	float NearClipSafeCm = 50.f;
};

/** Raised when the camera decides to turn the stick's reference direction (camera.md §5). Manual orbit never raises it */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraFrameChange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Frame")
	FVector FromForward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Frame")
	FVector ToForward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Frame")
	EGSTN_CameraFrameChangeReason Reason = EGSTN_CameraFrameChangeReason::Other;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Frame")
	bool bCut = false;
};

/** What a crosshair presenter shows (camera.md §6) */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CrosshairView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Crosshair")
	FVector2D ScreenPosition = FVector2D(0.5, 0.5);

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Crosshair")
	FVector HitPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Crosshair", meta = (ForceUnits = "cm"))
	float DistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Camera|Crosshair")
	bool bConverged = false;
};

/** The presented state of the view target, as the subject adapters report it (camera.md §8). Plain data */
USTRUCT(BlueprintType)
struct GSTNCAMERACORE_API FGSTN_CameraSubjectState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	bool bValid = false;

	/** The presented capsule point, never a mesh or a bone */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	FVector Pivot = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	FVector Up = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	FVector Velocity = FVector::ZeroVector;

	/** The direction the character faces, for recentre */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	FVector Facing = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	bool bGrounded = true;

	/** KO, shattering: the camera holds its last view */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera|Subject")
	bool bSuppressed = false;
};
