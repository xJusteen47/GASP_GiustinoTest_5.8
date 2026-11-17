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
#include "GSTN_CameraAnchor.generated.h"

class USceneComponent;

/** How the anchor reaches the view */
UENUM(BlueprintType)
enum class EGSTN_CameraAnchorMode : uint8
{
	/** Moves the camera component itself (GameplayCamera / spring arm / camera) each frame: GASP's rig, its damping and its collision see the anchor */
	MoveCameraComponent,
	/** Leaves the component alone and shifts the final view by the same correction (camera modifier). Fallback when no camera component is found */
	ShiftView,
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

	/** Horizontal damping half life. 0 = the camera follows the drawn body horizontally exactly (GASP's own damping still applies) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor|Horizontal", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float HorizontalHalfLife = 0.f;

	/** A jump of the anchor larger than this in one frame is a teleport (respawn, reset): the camera snaps instead of flying there */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anchor", meta = (ClampMin = "1", ForceUnits = "cm"))
	float SnapDistance = 400.f;
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

	/** Snaps to Target and clears the speeds */
	void Reset(const FVector& Target);

	/** Moves the damped location toward Target for DeltaTime and returns it. Up is the unit "up" (gravity opposite) the vertical rules use */
	FVector Step(const FVector& Target, const FVector& Up, float DeltaTime, const FGSTN_CameraAnchorSettings& Settings);

	/** One critically damped spring step (Holden's exact form) of X toward Goal; InOutSpeed is the spring's speed. HalfLife <= 0 snaps */
	static double SpringStep(double X, double Goal, double& InOutSpeed, float HalfLife, float DeltaTime);

	/**
	 * Where the actor root would be with the standing capsule, read back from the drawn mesh: the mesh world transform minus its standing
	 * relative location (the template's, which FGSTN_BodyCapsuleModifier offsets by the capsule change). Unchanged when only the capsule moves
	 */
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

	/** The damped anchor of the last frame (debug and tests) */
	FVector GetDampedAnchor() const { return Damper.Location; }

	/** The correction applied this frame: damped anchor minus what the camera component would follow without it */
	FVector GetLastCorrection() const { return LastCorrection; }

	/** True when the anchor runs for this owner: enabled, the console switch on, and the owner viewed by a local player */
	bool IsActiveForOwner() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveComponents();
	void RestoreCamera();

	TWeakObjectPtr<USceneComponent> CameraComponent;
	TWeakObjectPtr<USceneComponent> VisualComponent;
	FTransform CameraRelativeStanding = FTransform::Identity;
	FTransform VisualRelativeStanding = FTransform::Identity;
	bool bCameraFollowsVisual = false;
	bool bResolved = false;
	bool bCameraMoved = false;
	FGSTN_CameraAnchorDamper Damper;
	FVector LastCorrection = FVector::ZeroVector;
	bool bShiftView = false;
};
