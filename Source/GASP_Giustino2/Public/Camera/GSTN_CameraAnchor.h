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

	/** On/off (console gstn.Camera.TiltShift 0/1/2 overrides it for A/B: 0 off, 1 Cinematic DOF, 2 screen band, -1 = this data) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift")
	bool bEnabled = true;

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

	/** Band: the post process material (parameters BandCenter, BandHalfHeight, BandFeather, Radius). Missing = falls back to Cinematic DOF */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift|Band", meta = (EditCondition = "bEnabled"))
	TSoftObjectPtr<UMaterialInterface> BandMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Blueprints/Camera/M_GSTN_TiltShift.M_GSTN_TiltShift")));
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
	EGSTN_CameraVerticalSource VerticalSource = EGSTN_CameraVerticalSource::CapsuleBottom;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|TiltShift")
	FGSTN_CameraTiltShiftSettings TiltShift;
};

/** Shared default tuning. The camera anchor subsystem loads /Game/Blueprints/Data/DA_CameraAnchor_Default when it exists, else C++ defaults */
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

	static FVector StandingRootFromVisual(const FTransform& VisualWorld, const FTransform& VisualStandingRelative, const FVector& ParentScale = FVector::OneVector);
};

/**
 * Local presentation component on the viewed pawn: computes the damped capsule-independent anchor and moves the camera component there
 * (or exposes the correction for the camera modifier). Added at runtime by UGSTN_PlaytestCameraSubsystem to locally viewed pawns that do
 * not have one; a character Blueprint may place its own to give that character different settings. Never ticks on a dedicated server
 */
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
	static bool ComputeStandingRoot(const AActor* Actor, FVector& OutLocation, EGSTN_CameraVerticalSource VerticalSource = EGSTN_CameraVerticalSource::CapsuleBottom);

	/** The vertical source this frame: the console override (gstn.Camera.Anchor.Vertical 0 = settings, 1 = CapsuleBottom, 2 = DrawnMesh), else Settings */
	static EGSTN_CameraVerticalSource ResolveVerticalSource(EGSTN_CameraVerticalSource FromSettings);

	/** Replaces the height of a mesh-based standing root by the capsule-bottom one (CapsuleBottom), else returns it as is */
	static FVector ApplyVerticalSource(const AActor& Actor, const FVector& MeshStandingRoot, const FVector& Up, EGSTN_CameraVerticalSource VerticalSource);

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
