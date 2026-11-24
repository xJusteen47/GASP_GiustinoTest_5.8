// Camera anchor (owner 2026-11-17: "the camera shakes up and down on the stomp and when I go to the ground").
//
// Why the camera shook: GASP's camera (CameraAsset_SandboxCharacter, GameplayCameras) starts from its camera component, which follows the
// actor root. The root is the movement capsule's centre, and the combat body capsule moves it on purpose: a move's capsule shrinks or grows
// (dair tuck, knockdown, GroundedRestore growing the standing capsule back from the floor), the root jumps by the half height difference
// and FGSTN_BodyCapsuleModifier keeps the mesh where it was. The drawn body stays put, the camera jumps. The root also steps at the fixed
// sim rate (62.5 Hz) while the mesh is smoothed by Mover, another small vertical stair on fast falls.
//
// Fix: the camera follows a capsule-independent anchor, the "standing root" read back from the drawn mesh (where the root would be with the
// standing capsule, including Mover's smoothing), passed through a critically damped vertical spring whose values are data. Presentation
// only and local only: nothing here is read by the simulation, nothing is replicated. GASP's rigs stay untouched (its own DampenPosition
// still runs on top), so a GASP update still applies.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
class UGSTN_CombatSimComponent;

#include "GSTN_CameraAnchor.generated.h"

class USceneComponent;
class UMaterialInterface;

/** How the anchor reaches the view */
UENUM(BlueprintType)
enum class EGSTN_CameraAnchorMode : uint8
{
	/** Moves the camera component itself (GameplayCamera / spring arm / camera) each frame: GASP's rig, its damping and its collision see the anchor */
	MoveCameraComponent,
	/** Leaves the component alone and shifts the final view by the same correction (camera modifier). Fallback when no camera component is found */
	ShiftView,
};

/**
 * Where the anchor's HEIGHT comes from (owner 2026-11-20: no capsule change, from any source, may move the camera: crouch spam, aerials,
 * dropkick, knockdown, landing). The horizontal position always comes from the drawn, smoothed mesh
 */
UENUM(BlueprintType)
enum class EGSTN_CameraVerticalSource : uint8
{
	/**
	 * The capsule's BOTTOM plus the standing half height: on the ground the bottom never moves when the capsule changes (it is kept on the
	 * floor), so crouch, knockdown and landing capsules cannot move the camera at all, whatever the smoothing or the mesh does in between.
	 * Steps at the 62.5 Hz simulation rate on slopes and stairs: the vertical damping smooths it. In the air the air hold keeps the height
	 */
	CapsuleBottom,
	/** The drawn mesh read back to a standing root (first version): smooth, but follows any transient between capsule, mesh offset and smoothing */
	DrawnMesh,
	/**
	 * Default since 2026-11-23 (owner: "solve the camera anchored to the collision once and for all"). The camera's height never reads the
	 * capsule's size or place. On the ground: the FLOOR under the character (a trace from the capsule centre with the capsule's own collision
	 * responses) plus the standing half height. In the air: the drawn body (mesh standing root + the take off bias, as CapsuleBottom does) but
	 * never below floor + standing half height when a floor is within FloorProbeDistance.
	 * Why CapsuleBottom was not enough (dropkick K6, frames.jsonl 2026-11-23): in the air the mesh standing root is the CLIP's root, which the
	 * dropkick loop carries ~55 cm under the body; when the small torso capsule touched down, the height switched from that root to the capsule
	 * bottom in one frame (+57 cm), after the camera had followed the root down below the take off height. Any capsule whose bottom leaves the
	 * floor while Mover still reports ground (a capsule lifted onto the torso) moved the camera the same way. The floor has neither problem.
	 * No floor in reach = CapsuleBottom
	 */
	Floor,
};

/** How the tilt-shift "toy / miniature" look is made (GSTN_CameraAnchor.h, FGSTN_CameraTiltShiftSettings) */
UENUM(BlueprintType)
enum class EGSTN_TiltShiftMode : uint8
{
	/** Cinematic depth of field focused on the character with a very shallow focus (big virtual sensor): best look, real depth. Costs what
	 *  Cinematic DOF costs; on the Low scalability preset the engine turns DOF off, so a toaster pays nothing (and sees no effect) */
	CinematicDOF,
	/** A cheap screen-space band: sharp around the character's height on screen, blurred toward the top and bottom of the frame (post process
	 *  material M_GSTN_TiltShift, ~12 texture reads per blurred pixel, made by Tools/GASNPPTest/camera_tiltshift_setup.py). Works on any preset */
	ScreenBand,
	/**
	 * The real TILT (Scheimpflug): the plane of focus goes through the character but is rotated by TiltAngle around the camera's right axis,
	 * instead of staying parallel to the sensor. Blur grows with each pixel's distance from that plane (post process material M_GSTN_TiltPlane,
	 * same cost as the band, reads scene depth). Missing material = Cinematic DOF
	 */
	TiltedPlane,
};

/**
 * The tilt-shift "toy / miniature" look (owner 2026-11-18: Astro Bot and Mario's toy feel comes from tilt shift). A real tilt-shift photo
 * makes a full-size scene read as a model because only a thin slice is sharp, which is what a macro lens on a small object does. Presentation
 * only, local only: applied by UGSTN_PlaytestCameraModifier on top of the view
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_CameraTiltShiftSettings
{
	GENERATED_BODY()

	/** On/off (console gstn.Camera.TiltShift overrides it for A/B: 0 off, 1 Cinematic DOF, 2 screen band, 3 tilted plane, -1 = this data). Off by default: owner 2026-11-21, the toy look comes without blur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift", meta = (EditCondition = "bEnabled"))
	EGSTN_TiltShiftMode Mode = EGSTN_TiltShiftMode::CinematicDOF;

	/** Overall strength. 0 = no blur, 1 = the tuned look, 2 = twice the blur. In DOF mode it divides the f-stop, in band mode it scales the radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift", meta = (ClampMin = "0", ClampMax = "3", EditCondition = "bEnabled"))
	float Intensity = 1.f;

	/** DOF: focus distance added to the camera-to-character distance (+ focuses behind the character, - in front) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|DOF", meta = (ForceUnits = "cm", EditCondition = "bEnabled"))
	float FocusOffset = 0.f;

	/** DOF: aperture at Intensity 1. Lower = shallower focus, more blur */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|DOF", meta = (ClampMin = "0.7", ClampMax = "32", EditCondition = "bEnabled"))
	float Fstop = 4.f;

	/**
	 * DOF: virtual sensor width. The trick of the miniature look: at the same field of view a bigger sensor means a longer lens and a much
	 * shallower focus (blur grows about linearly with it). 24.89 mm is the engine default and gives almost no blur at playtest distances
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|DOF", meta = (ClampMin = "10", ClampMax = "1000", ForceUnits = "mm", EditCondition = "bEnabled"))
	float SensorWidth = 120.f;

	/** Band: half height of the sharp band around the character, as a fraction of the screen height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Band", meta = (ClampMin = "0", ClampMax = "0.5", EditCondition = "bEnabled"))
	float BandHalfHeight = 0.25f;

	/** Band: how far (fraction of the screen height) the blur takes to reach full strength past the sharp band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Band", meta = (ClampMin = "0.01", ClampMax = "1", EditCondition = "bEnabled"))
	float BandFeather = 0.35f;

	/** Band: largest blur radius in pixels at Intensity 1 (1080p reference; the material scales it with the view height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Band", meta = (ClampMin = "0", ClampMax = "40", EditCondition = "bEnabled"))
	float BandMaxRadius = 3.f;

	/**
	 * Tilted plane: rotation of the plane of focus from "parallel to the sensor" (0, ordinary depth of field) around the camera's right axis.
	 * Negative leans the plane's top toward the camera: with a camera looking down, the plane cuts the ground more steeply and the sharp slice
	 * of the ground gets thinner (the "reverse tilt" of miniature photos). Positive leans it toward the ground: more of the floor stays sharp
	 * (what architecture photographers use tilt for). At 90 - pitch the plane lies on a flat floor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Plane", meta = (ClampMin = "-80", ClampMax = "80", Units = "deg", EditCondition = "bEnabled"))
	float TiltAngle = -20.f;

	/** Tilted plane: blur radius in pixels (1080p) per unit of (distance from the plane / distance from the camera), at Intensity 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Plane", meta = (ClampMin = "0", ClampMax = "200", EditCondition = "bEnabled"))
	float PlaneBlurScale = 12.f;

	/** Tilted plane: blur radius cap in pixels (1080p) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Plane", meta = (ClampMin = "0", ClampMax = "40", EditCondition = "bEnabled"))
	float PlaneMaxRadius = 4.f;

	/** Tilted plane material (parameters PlanePoint and PlaneNormal camera relative, BlurScale, Radius). Missing = Cinematic DOF */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Plane", meta = (EditCondition = "bEnabled"))
	TSoftObjectPtr<UMaterialInterface> PlaneMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Blueprints/Camera/M_GSTN_TiltPlane.M_GSTN_TiltPlane")));

	/** Band: the post process material (parameters BandCenter, BandHalfHeight, BandFeather, Radius). Missing = falls back to Cinematic DOF */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Band", meta = (EditCondition = "bEnabled"))
	TSoftObjectPtr<UMaterialInterface> BandMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Blueprints/Camera/M_GSTN_TiltShift.M_GSTN_TiltShift")));
};

/**
 * The SHIFT half of a tilt-shift lens (owner 2026-11-18: tilt and shift are two independent things). Shift slides the lens parallel to the
 * sensor: the camera keeps its orientation and the picture is reframed (off-axis projection). It changes no focus and no blur. Its classic use
 * is perspective control: a camera kept level does not make verticals converge, and the shift brings the ground back into the frame.
 * Presentation only: applied by UGSTN_PlaytestCameraModifier through the view's off-center projection
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_CameraShiftSettings
{
	GENERATED_BODY()

	/** On/off (console gstn.Camera.Shift 0/1 overrides it, -1 = this data) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shift")
	bool bEnabled = false;

	/**
	 * Straighten verticals: 0 = the camera keeps its pitch (no change), 1 = the camera is levelled (verticals parallel) and the lens shift
	 * keeps the character exactly where it was on screen. In between, part of the pitch becomes shift. The ground near the character looks the
	 * same; walls and towers stop leaning, the top of the frame shows less sky
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shift", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bEnabled"))
	float StraightenVerticals = 0.5f;

	/** Extra manual shift, in half screens (X right, Y up), on top of the straightening. Moves the framing without turning the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shift", meta = (ClampMin = "-1", ClampMax = "1", EditCondition = "bEnabled"))
	FVector2D Offset = FVector2D::ZeroVector;
};

/** Tuning of the camera anchor. Data, never project settings: on the component (per character) or in a UGSTN_CameraAnchorSettingsAsset */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_CameraAnchorSettings
{
	GENERATED_BODY()

	/** Off = the camera follows the actor root as GASP does (A/B compare; console gstn.Camera.Anchor 0 does the same for everyone) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor")
	EGSTN_CameraAnchorMode Mode = EGSTN_CameraAnchorMode::MoveCameraComponent;

	/** Where the anchor's height comes from. Console override gstn.Camera.Anchor.Vertical (A/B) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical")
	EGSTN_CameraVerticalSource VerticalSource = EGSTN_CameraVerticalSource::Floor;

	/**
	 * Floor source: how far below the capsule's bottom the floor is searched. In the air a floor farther than this does not hold the camera up
	 * (a fall off a ledge is followed down as before until the new floor comes in reach). On the ground the floor is always well inside it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical", meta = (ClampMin = "10", ClampMax = "2000", ForceUnits = "cm",
		EditCondition = "VerticalSource == EGSTN_CameraVerticalSource::Floor"))
	float FloorProbeDistance = 300.f;

	/**
	 * Floor source, in the air: the height never goes below floor + standing half height. A body cannot be under the floor, so a lower value
	 * is the clip's root or a move's capsule, never real motion (dropkick, dair, knockdown bounces). Off = the drawn body only (A/B)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical", meta = (EditCondition = "VerticalSource == EGSTN_CameraVerticalSource::Floor"))
	bool bFloorClampInAir = true;

	/**
	 * Vertical spring half life when the anchor goes UP (jumps, standing up). Higher = softer. After a sudden step the camera has covered
	 * about 40% of it in one half life, 92% in three. 0 = no damping
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float UpHalfLife = 0.07f;

	/** Same, when the anchor goes DOWN (stomp landing, knockdown, falls). 0 = no damping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float DownHalfLife = 0.07f;

	/** Vertical moves of the anchor smaller than this do not move the camera at all (0 = off). Larger ones are followed minus the band */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical", meta = (ClampMin = "0", ClampMax = "200", ForceUnits = "cm"))
	float DeadBand = 0.f;

	/** The camera never trails the anchor vertically by more than this (fast falls stay framed). 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Vertical", meta = (ClampMin = "0", ClampMax = "1000", ForceUnits = "cm"))
	float MaxVerticalLag = 80.f;

	/**
	 * Mario / Astro Bot rule (owner 2026-11-18: their camera does not blindly follow the character): while airborne the camera keeps the
	 * height of the ground the character left, so an ordinary jump does not move it at all. It rises only by what the character climbs above
	 * AirRiseBand (very high jumps, wall climbs), follows down whatever goes below the take-off height (falls off a ledge, to see the landing),
	 * and re-anchors to the new ground on landing with LandingHalfLife (a jump onto a higher platform). Off = the camera follows every jump
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Air")
	bool bHoldGroundHeightInAir = true;

	/** How far above the take-off ground the character may go before the camera starts to rise (the rest of the climb is followed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Air", meta = (ClampMin = "0", ClampMax = "2000", ForceUnits = "cm", EditCondition = "bHoldGroundHeightInAir"))
	float AirRiseBand = 250.f;

	/** How far below the take-off ground the character may go before the camera follows down. 0 = follows any drop below it right away */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Air", meta = (ClampMin = "0", ClampMax = "2000", ForceUnits = "cm", EditCondition = "bHoldGroundHeightInAir"))
	float AirFallBand = 0.f;

	/** Half life of the vertical catch up after a landing on a different height (softer than the capsule damping above). 0 = the normal half lives */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Air", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s", EditCondition = "bHoldGroundHeightInAir"))
	float LandingHalfLife = 0.2f;

	/** Look ahead: the camera aims this many seconds ahead of the character's horizontal velocity (0 = off). Try 0.25 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Horizontal", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float LookAheadTime = 0.f;

	/** Look ahead never exceeds this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Horizontal", meta = (ClampMin = "0", ClampMax = "500", ForceUnits = "cm"))
	float MaxLookAhead = 80.f;

	/** How slowly the look ahead turns and grows (half life): high keeps it from swinging on every direction change */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Horizontal", meta = (ClampMin = "0", ClampMax = "3", ForceUnits = "s"))
	float LookAheadHalfLife = 0.5f;

	/** Horizontal damping half life. 0 = the camera follows the drawn body horizontally exactly (GASP's own damping still applies) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Horizontal", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float HorizontalHalfLife = 0.f;

	/** A jump of the anchor larger than this in one frame is a teleport (respawn, reset): the camera snaps instead of flying there */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor", meta = (ClampMin = "1", ForceUnits = "cm"))
	float SnapDistance = 400.f;

	/**
	 * Crouch lowers the camera only while aiming (owner 2026-11-18: no crouch offset in the normal view, "in ADS it would be right, I must see
	 * it"). GASP's own crouch offset rig (CameraRig_CrouchOffset) is zeroed in every mode; this puts AimCrouchOffset back while the pawn is
	 * crouched AND aims (GASP's PlayerInputState.WantsToAim). Off = crouch never moves the camera
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Crouch")
	bool bCrouchOffsetInAim = true;

	/** The aim crouch offset: X along the view's forward (yaw only), Y right, Z along up. GASP's rig had (40, 0, -30) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Crouch", meta = (ForceUnits = "cm", EditCondition = "bCrouchOffsetInAim"))
	FVector AimCrouchOffset = FVector(40.0, 0.0, -30.0);

	/** How fast the aim crouch offset comes in and goes out (half life) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Crouch", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s", EditCondition = "bCrouchOffsetInAim"))
	float AimCrouchHalfLife = 0.1f;

	/**
	 * Distance pushed behind GASP's view along its forward (owner 2026-11-17 "much farther" = 550, 2026-11-18 "a bit closer"). Stops short of
	 * walls. Console gstn.Camera.PlaytestExtraDistance >= 0 overrides it (A/B); -1 = this data
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "0", ClampMax = "2000", ForceUnits = "cm"))
	float ExtraDistance = 400.f;

	/** Extra height on top of ExtraDistance. Console gstn.Camera.PlaytestExtraHeight overrides it unless it is -1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "-500", ClampMax = "1000", ForceUnits = "cm"))
	float ExtraHeight = 45.f;

	/**
	 * Telephoto framing (miniature optics): a narrower field of view with the camera moved back so the character keeps the same size and
	 * place on screen. Flatter perspective, less wide-angle stretch, the world reads more like a model. Horizontal degrees; 0 = off (GASP's
	 * FOV). Only narrows: a value above the current FOV does nothing. Try 55, then 45 (Super Mario Galaxy's default). If a wall stops the
	 * camera short, the FOV opens again to keep the character's size
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Framing", meta = (ClampMin = "0", ClampMax = "120", Units = "deg"))
	float TelephotoFOV = 0.f;

	/** TILT half (focus): which slice of the world is sharp. Independent from Shift */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift", meta = (DisplayName = "Tilt (focus)"))
	FGSTN_CameraTiltShiftSettings TiltShift;

	/** SHIFT half (framing): off-axis projection. Independent from the tilt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shift")
	FGSTN_CameraShiftSettings Shift;
};

/** Shared default tuning. The camera anchor subsystem loads /Game/GSTN/Data/Camera/DA_CameraAnchor_Default when it exists, else C++ defaults */
UCLASS(BlueprintType)
class GASP_GIUSTINO2_API UGSTN_CameraAnchorSettingsAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Anchor", meta = (ShowOnlyInnerProperties))
	FGSTN_CameraAnchorSettings Settings;
};

/** The damped anchor state and its pure step (tests drive it without a world) */
USTRUCT()
struct GASP_GIUSTINO2_API FGSTN_CameraAnchorDamper
{
	GENERATED_BODY()

	FVector Location = FVector::ZeroVector;
	/** Velocity along Up only (the vertical spring) */
	double VerticalSpeed = 0.0;
	FVector HorizontalSpeed = FVector::ZeroVector;
	bool bValid = false;
	/** Height (along Up) of the last ground the character stood on, for the air hold */
	double GroundHeight = 0.0;
	bool bHasGround = false;
	bool bWasInAir = false;
	/** Set on landing, cleared once the camera reached the new ground: the vertical spring uses LandingHalfLife meanwhile */
	bool bLandingCatchUp = false;

	/** Snaps to Target and clears the speeds */
	void Reset(const FVector& Target);

	/**
	 * Moves the damped location toward Target for DeltaTime and returns it. Up is the unit "up" (gravity opposite) the vertical rules use.
	 * bInAir drives the air hold (bHoldGroundHeightInAir): false = grounded, Target's height becomes the ground height
	 */
	FVector Step(const FVector& Target, const FVector& Up, float DeltaTime, const FGSTN_CameraAnchorSettings& Settings, bool bInAir = false);

	/** The height the camera follows given the wanted height (air hold applied). Updates the ground height when grounded. Pure helper of Step */
	double AirHoldHeight(double Wanted, bool bInAir, const FGSTN_CameraAnchorSettings& Settings);

	/** One critically damped spring step (Holden's exact form) of X toward Goal; InOutSpeed is the spring's speed. HalfLife <= 0 snaps */
	static double SpringStep(double X, double Goal, double& InOutSpeed, float HalfLife, float DeltaTime);

	/**
	 * Where the actor root would be with the standing capsule, read back from the drawn mesh: the mesh world transform minus its standing
	 * relative location (the template's, which FGSTN_BodyCapsuleModifier offsets by the capsule change). Unchanged when only the capsule moves
	 */
	/**
	 * The standing root's height along Up from the capsule: its bottom (Root - Up * CurrentHalfHeight) raised by the standing half height. Root is
	 * the capsule centre. Independent of every capsule size change that keeps the bottom where it is
	 */
	static double StandingHeightFromCapsule(const FVector& Root, const FVector& Up, double CurrentHalfHeight, double StandingHalfHeight);

	/**
	 * The Floor source's height (EGSTN_CameraVerticalSource::Floor), pure. Grounded: FloorHeight + StandingHalfHeight, or CapsuleFallback without a
	 * floor. In the air: MeshHeight + AirBias, raised to FloorHeight + StandingHalfHeight when bClampInAir and a floor was found. All along Up
	 */
	static double FloorSourceHeight(bool bInAir, double MeshHeight, double AirBias, bool bHasFloor, double FloorHeight, double StandingHalfHeight,
		double CapsuleFallback, bool bClampInAir);

	static FVector StandingRootFromVisual(const FTransform& VisualWorld, const FTransform& VisualStandingRelative, const FVector& ParentScale = FVector::OneVector);
};

/**
 * Local presentation component on the viewed pawn: computes the damped capsule-independent anchor and moves the camera component there
 * (or exposes the correction for the camera modifier). Added at runtime by UGSTN_PlaytestCameraSubsystem to locally viewed pawns that do
 * not have one; a character Blueprint may place its own to give that character different settings. Never ticks on a dedicated server
 */
/**
 * The presented capsule changed size (crouch, aerial, landing, knockdown, any body capsule track): half heights before and after, and how far its
 * bottom moved along Up (0 on the ground: the bottom stays on the floor). Presentation, once per rendered frame with a change, never during a
 * rollback re-simulation: what the screen shows, not each simulated step
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSTN_OnCameraCapsuleChanged, float, OldHalfHeight, float, NewHalfHeight, float, BottomShift);

class UGSTN_CameraAnchorComponent;
/**
 * Native: the anchor finished this frame's update (TG_PostPhysics, before every camera that ticks in that group: they take the anchor as a
 * prerequisite). A camera that is handed its subject from outside (the P1a GSTN camera) gets it here, the same frame, instead of from a
 * tickable that runs after the view (the one-frame-late pivot noted on 2026-11-21)
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnCameraAnchorUpdated, const UGSTN_CameraAnchorComponent&);

UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class GASP_GIUSTINO2_API UGSTN_CameraAnchorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_CameraAnchorComponent();

	/** Per character tuning. Used as is unless SettingsAsset is set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor")
	FGSTN_CameraAnchorSettings Settings;

	/** Optional shared tuning; wins over Settings when set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor")
	TObjectPtr<UGSTN_CameraAnchorSettingsAsset> SettingsAsset;

	/**
	 * Observer for custom cameras (owner 2026-11-20): fires when the drawn capsule changes size. The anchor already ignores these changes by
	 * itself (VerticalSource); bind this only to react on purpose (e.g. a camera that WANTS to dip on crouch)
	 */
	UPROPERTY(BlueprintAssignable, Category = "Camera|Anchor")
	FGSTN_OnCameraCapsuleChanged OnCapsuleChanged;

	/** See FGSTN_OnCameraAnchorUpdated. Fires at the end of every active tick */
	FGSTN_OnCameraAnchorUpdated OnAnchorUpdated;

	/** A component with this tag is the one moved; else the first GameplayCamera, then spring arm, then camera component of the owner */
	static const FName CameraComponentTag;

	UFUNCTION(BlueprintPure, Category = "Camera|Anchor")
	const FGSTN_CameraAnchorSettings& GetSettings() const;

	/** World offset to add to the view when the mode is ShiftView (or no camera component was found). Zero otherwise */
	FVector GetViewShift() const;

	/**
	 * The damped anchor of this frame (valid from TG_PostPhysics on; before that, last frame's): the standing root with the camera's damping and
	 * air hold, without look ahead or aim offsets. Free of capsule size changes and of simulation step jitter
	 */
	UFUNCTION(BlueprintPure, Category = "Camera|Anchor")
	FVector GetDampedAnchor() const { return Damper.Location; }

	/** The undamped standing root this frame's anchor read (see UGSTN_CameraAnchorLibrary::GetCameraPivot) */
	UFUNCTION(BlueprintPure, Category = "Camera|Anchor")
	FVector GetStandingRoot() const { return LastStandingRoot; }

	/** Diagnostics (gstn.Camera.Trace): the component the anchor moves (null = ShiftView), and the air state it read this frame */
	USceneComponent* GetMovedCameraComponent() const { return CameraComponent.Get(); }
	bool WasInAirLastTick() const { return bLastInAir; }

	/** True once the damper holds a value (it resets while the anchor is off) */
	bool HasDampedAnchor() const { return Damper.bValid; }

	/**
	 * The standing root of Actor as drawn this frame: where its actor root would be with its own (standing) capsule, read back from the drawn,
	 * Mover-smoothed mesh. Capsule resizes and the 62.5 Hz simulation steps do not move it; the body's real motion does. Runs the body capsule's
	 * mesh correction first, so it is right at any point of the frame after Mover's finalize. False (actor location returned) without a mesh
	 */
	static bool ComputeStandingRoot(const AActor* Actor, FVector& OutLocation, EGSTN_CameraVerticalSource VerticalSource = EGSTN_CameraVerticalSource::Floor);

	/** The vertical source this frame: the console override (gstn.Camera.Anchor.Vertical 0 = settings, 1 = CapsuleBottom, 2 = DrawnMesh, 3 = Floor), else Settings */
	static EGSTN_CameraVerticalSource ResolveVerticalSource(EGSTN_CameraVerticalSource FromSettings);

	/**
	 * Replaces the height of a mesh-based standing root by the capsule-bottom one (CapsuleBottom, ON THE GROUND only), else returns it as is.
	 * In the air an aerial's capsule shrinks around the body (owner 2026-11-20, FAir/dair): its bottom rises, so the bottom is no reference
	 * there; the mesh-based height is, plus AirBias (the bottom-minus-mesh difference of the last grounded frame, so take off does not step)
	 */
	static FVector ApplyVerticalSource(const AActor& Actor, const FVector& MeshStandingRoot, const FVector& Up, EGSTN_CameraVerticalSource VerticalSource,
		bool bInAir = false, double AirBias = 0.0, const FGSTN_CameraAnchorSettings* Tuning = nullptr);

	/**
	 * The floor under the actor's capsule, along Up: a line from the capsule centre down to FloorProbeDistance below its bottom, on the capsule's
	 * object channel with its own responses (what Mover's floor check collides with), ignoring the actor. False when nothing is in reach
	 */
	static bool FindFloorHeight(const AActor& Actor, const FVector& Up, float FloorProbeDistance, double& OutFloorHeight);

	/** The standing (pawn's own) capsule half height, scaled: the body capsule modifier's reference, else the capsule archetype's, else the current one */
	static double GetStandingHalfHeight(const AActor& Actor);

	/** Diagnostics: the mesh-based standing root before the vertical source (same as GetStandingRoot with DrawnMesh) */
	FVector GetMeshStandingRoot() const { return LastMeshStandingRoot; }


	/** The correction applied this frame: damped anchor minus what the camera component would follow without it */
	FVector GetLastCorrection() const { return LastCorrection; }

	/** True when the anchor runs for this owner: enabled, the console switch on, and the owner viewed by a local player */
	bool IsActiveForOwner() const;

	/**
	 * The tick group the anchor runs in: the last one before UWorld::Tick updates the camera managers, so the view of a frame is built from
	 * that frame's anchor. Mover's state is finalized and smoothed earlier (Network Prediction, world pre actor tick)
	 */
	static constexpr ETickingGroup EarlyTickGroup = TG_PostPhysics;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveComponents();
	void RestoreCamera();
	/** Tick group and prerequisites for gstn.Camera.Anchor.Early (1 = before the view, 0 = the first version's TG_PostUpdateWork) */
	void SetTickOrder(bool bEarly);

	TWeakObjectPtr<USceneComponent> CameraComponent;
	TWeakObjectPtr<USceneComponent> VisualComponent;
	TWeakObjectPtr<UGSTN_CombatSimComponent> CombatComponent;
	bool bOrderSet = false;
	bool bOrderEarly = true;
	FTransform CameraRelativeStanding = FTransform::Identity;
	FTransform VisualRelativeStanding = FTransform::Identity;
	bool bCameraFollowsVisual = false;
	bool bResolved = false;
	bool bCameraMoved = false;
	FGSTN_CameraAnchorDamper Damper;
	FVector LastCorrection = FVector::ZeroVector;
	FVector LastStandingRoot = FVector::ZeroVector;
	FVector LastMeshStandingRoot = FVector::ZeroVector;
	bool bLastInAir = false;
	/** Capsule-bottom height minus mesh height on the last grounded frame (cm, along Up): carried through the air */
	double AirBias = 0.0;
	/** Last presented capsule (OnCapsuleChanged): half height and bottom height along Up; < 0 = not read yet */
	float LastCapsuleHalfHeight = -1.f;
	double LastCapsuleBottom = 0.0;
	bool bShiftView = false;
	/** 0..1 blend of the aim crouch offset */
	float AimCrouchAlpha = 0.f;
	/** Smoothed look ahead offset (world, horizontal) */
	FVector LookAhead = FVector::ZeroVector;

	/** GASP's PlayerInputState.WantsToAim of the owner (a Blueprint struct: found by name, GUID suffix ignored), false when absent */
	bool IsOwnerAiming() const;
	/** The world offset of the aim crouch this frame (already blended) */
	FVector StepAimCrouchOffset(float DeltaTime, const FVector& Up, const FGSTN_CameraAnchorSettings& Tuning);
};
