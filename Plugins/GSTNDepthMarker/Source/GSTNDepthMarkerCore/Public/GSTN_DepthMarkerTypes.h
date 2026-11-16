// Depth marker: pure data types (depth-marker.md §5). Presentation only: nothing here is simulated or replicated.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectMacros.h"
#include "GSTN_DepthMarkerTypes.generated.h"

/** What a target is made of */
UENUM(BlueprintType)
enum class EGSTN_DepthTargetKind : uint8
{
	None,
	/** A real surface found by a trace */
	Surface,
	/** The reference plane (the last platform's surface or a reference volume): the "no ground" ring or the "above" ring */
	ReferencePlane,
	/** The plane of a nearby higher platform, seen from here: marker B of the approach layer (the above layer is a different one) */
	ApproachPlane
};

/** Visual state of one target (§2.3 step 9) */
UENUM(BlueprintType)
enum class EGSTN_DepthMarkerState : uint8
{
	Hidden,
	/** A surface directly under the feet */
	Ground,
	/** The centre ray left the surface but the footprint still touches it (R3) */
	EdgeHold,
	/** Nothing below and the entity is above the reference plane: the hollow ring below (§2.7) */
	NoGround,
	/** Below the reference plane: the ring above the entity, line going up */
	AboveReference,
	/** The character column projected onto a nearby higher platform top plane: it hangs in the air beyond the platform edge */
	Approach
};

/**
 * What the subject adapter reports each frame, in world space and from the PRESENTED transform (§3, §5). Read from components
 * on the machine that draws; the simulation never sees it.
 */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthSubjectState
{
	GENERATED_BODY()

	/** Foot point (presented), never the mesh bone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	FVector Feet = FVector::ZeroVector;

	/** Unit up: the opposite of the entity's local gravity this frame. Never world Z by assumption */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	FVector Up = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	bool bAirborne = false;

	/** Feeds the last grounded point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	bool bGrounded = false;

	/** Footprint sweep radius and decal scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FootRadius = 30.f;

	/** Height of the head over the feet: where the up line starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float TopOffset = 180.f;

	/** Unused by the v1 selector; kept for a later landing selector */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	FVector Velocity = FVector::ZeroVector;

	/** What the entity stands on, for bMovingBase and the grounded point */
	UPROPERTY(BlueprintReadWrite, Category = "DepthMarker|Subject")
	TWeakObjectPtr<UObject> BaseComponent;

	/** The base moves (lift, enemy platform): the grounded point is not updated from it unless the params allow it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	bool bBaseMoves = false;

	/** Wall mode, derived from mode names given as data on the adapter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	bool bOnWall = false;

	/** Linked or parked: the marker is hidden (LinkPolicy) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	bool bParked = false;

	/** Dead or hidden: the marker fades out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	bool bSuppressed = false;

	/** Optional, for the audience (INDEX_NONE = unknown) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Subject")
	FLinearColor Tint = FLinearColor::White;
};

/** One trace hit reduced to numbers (§2.3 step 4). Candidates arrive from any source: a line trace, a test */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthCandidate
{
	GENERATED_BODY()

	/** Distance from the feet to the hit along Down (negative: the hit is above the feet) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	float Height = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	FVector Point = FVector::ZeroVector;

	/** Face normal of the hit (the side the trace saw) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	FVector Normal = FVector::UpVector;

	/** Opaque identity of the surface (the hit component, hashed); 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	int64 SurfaceKey = 0;

	/** Transform of the hit component when the trace ran, to store the plane in its local space (§2.3 step 7) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	FTransform ComponentTransform = FTransform::Identity;

	/** Behaviours the surface offers (Surface.Behaviour.*), if a resolver knows them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	FGameplayTagContainer BehaviourTags;

	/** False when no resolver knew the surface: it passes if the params say unknown surfaces pass */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	bool bSurfaceKnown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	bool bBlocking = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	bool bStartPenetrating = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	bool bMovingBase = false;

	/** Pawn, projectile, ignored subject, non-target: excluded by the caller from settings (§2.8) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	bool bExcluded = false;
};

/**
 * One candidate projection plane for the approach marker: a walkable top found by a probe near the character, or a plane a
 * designer declared (volume or component). Heights, distances and approach speed are computed from the subject each frame,
 * so a cached candidate stays valid while the character moves.
 */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthPlaneCandidate
{
	GENERATED_BODY()

	/** A point of the platform top: the sampled hit, or for a designer plane any point of it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Approach")
	FVector Point = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Approach")
	FVector Normal = FVector::UpVector;

	/** Opaque identity of the platform (the hit component, hashed); 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Approach")
	int64 SurfaceKey = 0;

	/** Transform of the platform component, to re-express the plane on a moving base */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Approach")
	FTransform ComponentTransform = FTransform::Identity;

	/** Declared by a designer (volume or plane component) and overlapping the character: wins over probing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Approach")
	bool bExplicit = false;

	/** Among several overlapping designer planes the higher priority wins */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Approach")
	int32 Priority = 0;
};

/** The footprint sweep result of rule R3: does a short sphere sweep still touch the previously selected surface */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthFootprintHit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	int64 SurfaceKey = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate")
	FVector Point = FVector::ZeroVector;

	/** Distance of the impact from the centre column, perpendicular to Up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Candidate", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float LateralCm = 0.f;
};

/** A reference plane override, normally from a level-authored reference volume (§2.7) */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthReferenceOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Reference")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Reference")
	FVector Point = FVector::ZeroVector;

	/** The volume's own up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Reference")
	FVector Normal = FVector::UpVector;

	/** A pit: the ring can double as a warning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DepthMarker|Reference")
	bool bHazard = false;
};

/** One marker target (§5) */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	EGSTN_DepthTargetKind Kind = EGSTN_DepthTargetKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	EGSTN_DepthMarkerState State = EGSTN_DepthMarkerState::Hidden;

	/** On the surface or the plane, straight along the entity's column */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	FVector Point = FVector::ZeroVector;

	/** Surface normal, unit */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	FVector Normal = FVector::UpVector;

	/** Normal tilted at most MaxTiltDegrees from Up, for mesh presenters (§2.5) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	FVector PresentNormal = FVector::UpVector;

	/** Distance from the feet along Down to the point (for the Above target: along Up, positive) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	float Height = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	int64 SurfaceKey = 0;

	/** The plane in the hit component's local space (follows moving bases between traces, §2.3 step 7) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	FVector LocalPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	FVector LocalNormal = FVector::UpVector;

	/** Alpha factor of an edge hold (1 outside a hold) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	float EdgeAlpha = 1.f;

	/** 1/max(dot(Normal, Up), 0.5), capped at 2: the cosmetic stretch of a disc on a slope (§2.5) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	float SlopeRadiusScale = 1.f;

	/** The face is steeper than MinNormalDot: still a candidate, the style may tint it */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	bool bSteep = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target")
	bool bHazard = false;

	/** Approach target only: horizontal distance from the feet to the platform's nearest sampled point (0 for a designer plane and elsewhere) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target", meta = (ForceUnits = "cm"))
	float EdgeDistanceCm = 0.f;

	/** Cross-fade length when this target replaced another one (0 = none) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Target", meta = (ForceUnits = "ms"))
	float BlendMs = 0.f;

	bool IsValid() const { return Kind != EGSTN_DepthTargetKind::None; }
};

/** What a selector returns: the below and the above layer, independent (§2.7) */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Result")
	FGSTN_DepthTarget Below;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Result")
	FGSTN_DepthTarget Above;

	/** The approach marker B: the column projected onto a nearby higher platform plane (at most one) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Result")
	FGSTN_DepthTarget Approach;

	/** The gravity direction jumped this frame: the presenter cross-fades over GravityFadeMs and drops its line history */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Result")
	bool bGravityReset = false;

	/** The selection state was cleared (teleport or gravity jump): no blend */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|Result")
	bool bStateCleared = false;
};

/** Per marker memory (§5). Local to the machine; plain data so a test can build one */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthSelectionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bHasPrevious = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	int64 PreviousKey = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FVector PreviousPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FVector PreviousNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	float PreviousHeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FTransform PreviousComponentTransform = FTransform::Identity;

	/** Time of the last switch (R4 dwell) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	double LastSwitchTime = -1000.0;

	/** The last switch was to a FARTHER surface (R4): only then does the dwell hold back a nearer one */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bLastSwitchWasFarther = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bHasGrounded = false;

	/** World point where the entity last stood on a static surface; the plane is rebuilt from it with the CURRENT Up */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FVector LastGroundedPoint = FVector::ZeroVector;

	/** Time since which the entity has been grounded on a static surface (negative: not) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	double GroundedSince = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bHasLastUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FVector LastUp = FVector::UpVector;

	/** The above ring is currently shown (hysteresis between AboveShowDepthCm and AboveHideDepthCm) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bAboveShown = false;

	/** The marker is currently visible (hysteresis between ShowHeight and HideHeight) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bVisible = false;

	/** The approach marker is currently shown, and the plane it holds (world point and normal, kept for the hold time) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bApproachShown = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	int64 ApproachKey = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	bool bApproachExplicit = false;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FVector ApproachPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FVector ApproachNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	FTransform ApproachTransform = FTransform::Identity;

	/** Time of the last switch of the approach plane (dwell) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	double ApproachSwitchTime = -1000.0;

	/** Time a candidate last confirmed the held plane (hold time while probes are staggered) */
	UPROPERTY(BlueprintReadOnly, Category = "DepthMarker|State")
	double ApproachLastSeenTime = -1000.0;

	void ClearApproach()
	{
		bApproachShown = false;
		ApproachKey = 0;
		bApproachExplicit = false;
		ApproachSwitchTime = -1000.0;
		ApproachLastSeenTime = -1000.0;
	}

	/** Forget the selection and the above latch (teleport, pool acquire, gravity jump). The grounded point stays unless asked */
	void ClearSelection()
	{
		ClearApproach();
		bHasPrevious = false;
		PreviousKey = 0;
		bLastSwitchWasFarther = false;
		LastSwitchTime = -1000.0;
		bAboveShown = false;
	}

	void ClearAll()
	{
		*this = FGSTN_DepthSelectionState();
	}
};

/** Tuning of §2, every named tolerance in one place. Defaults are the spec's; the settings hold the project's copy */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthSelectorParams
{
	GENERATED_BODY()

	/** Length of the gravity probe */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float ProbeLength = 6000.f;

	/** A hit up to this far above the feet still counts (a thin platform just passed upward is not chosen beyond it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PenetrationTolerance = 2.f;

	/** R1: height difference under which two candidates are the same surface (overlapping meshes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CoplanarTolerance = 2.f;

	/** R1: normal difference under which two candidates are the same surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float CoplanarAngleDegrees = 5.f;

	/** R3: lateral distance within which the footprint sweep keeps the previous surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float EdgeForgivenessCm = 40.f;

	/** Footprint sweep radius as a fraction of the subject's FootRadius */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float FootprintRadiusScale = 1.f;

	/** R4: no further switch to a farther surface before this long after the last switch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float MinDwellMs = 100.f;

	/** R4: cross-fade length of a switch to a farther surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float SwitchBlendMs = 80.f;

	/** Faces with dot(Normal, Up) below this are flagged steep (still candidates) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinNormalDot = 0.2f;

	/** Mesh presenters orient to the normal clamped to this angle from Up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float MaxTiltDegrees = 50.f;

	/** Lift along the normal against z-fighting */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SurfaceOffsetCm = 0.5f;

	/** A change of Up larger than this in one frame clears the selection and cross-fades */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float UpResetDegrees = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float GravityFadeMs = 120.f;

	/** The last grounded point updates only after standing this long on a static surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float GroundedMinMs = 150.f;

	/** The rings are shown while the entity is within this distance of the plane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ReferenceMaxDistanceCm = 4000.f;

	/** The above ring appears once the feet are this far below the plane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AboveShowDepthCm = 50.f;

	/** ... and hides above this depth (hysteresis against a landing on the lip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AboveHideDepthCm = 30.f;

	/** Riding an enemy that walks off a cliff must not move the lip, so moving bases do not update the grounded point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection")
	bool bUpdateFromMovingBases = false;

	/** Surfaces no resolver knows pass the behaviour filter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection")
	bool bUnknownSurfacesPass = true;

	// ---- Approach marker (marker B): the character is above a lower platform and a higher one is nearby, offset sideways ----

	/** Master switch of the approach layer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach")
	bool bApproachEnabled = true;

	/** A higher platform whose nearest sampled point is within this horizontal distance can show marker B */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachDistanceCm = 500.f;

	/** Once shown, marker B stays until the platform is farther than ApproachDistanceCm plus this (hysteresis) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachHideMarginCm = 100.f;

	/** Within this distance the platform counts as near even if the character is not moving towards it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachCloseDistanceCm = 150.f;

	/** Horizontal speed towards the platform that counts as approaching it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float ApproachMinTowardSpeed = 60.f;

	/** The plane must be at most this far above the feet: below or near the character (higher ones are the above layer job) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachNearAboveFeetCm = 60.f;

	/** The plane must be at least this far above the primary ground (a platform at the same level is not worth a second marker) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachMinRiseCm = 30.f;

	/** ... and at most this far above it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachMaxRiseCm = 1500.f;

	/** Probed tops must be this flat: dot(Normal, Up) at least this (a wall is no platform) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ApproachMinNormalDot = 0.7f;

	/** No switch from one approach plane to another before this long after the last one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float ApproachMinDwellMs = 200.f;

	/** A challenger must be this much nearer (edge distance) to take over after the dwell */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachSwitchMarginCm = 60.f;

	/** Probes are staggered, so a plane that no probe confirmed is held this long (while it stays in range) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float ApproachHoldMs = 300.f;

	/** Fade-in of marker B when it appears or switches plane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float ApproachBlendMs = 120.f;

	/** Downward probes on a ring around the character */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0", ClampMax = "16"))
	int32 ApproachRingProbes = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ApproachProbeRadiusCm = 350.f;

	/** Downward probes along the horizontal velocity (half and full ApproachDistanceCm); skipped when the character is slow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0", ClampMax = "4"))
	int32 ApproachVelocityProbes = 2;

	/** Probes issued per marker per tick; they rotate, so the whole pattern is covered over a few ticks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ApproachProbesPerTick = 3;

	/** How far below the probe start (which sits ApproachNearAboveFeetCm above the feet) each probe reaches */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float ApproachProbeDepthCm = 1500.f;

	/** A probe result is reused this long before it expires */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Approach", meta = (ClampMin = "0.0", ForceUnits = "ms"))
	float ApproachCacheMs = 600.f;
};

/** Visibility and fade over height (§3); the style's curves feed the presenters, these linear defaults are the maths fallback */
USTRUCT(BlueprintType)
struct GSTNDEPTHMARKERCORE_API FGSTN_DepthFadeParams
{
	GENERATED_BODY()

	/** Shown when airborne and at least this high over the target */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ShowHeight = 60.f;

	/** Hidden below this height (hysteresis) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float HideHeight = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float FadeEndHeight = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlphaNear = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AlphaFar = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "0.0"))
	float RadiusNear = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Fade", meta = (ClampMin = "0.0"))
	float RadiusFar = 0.6f;
};
