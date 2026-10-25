// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "MoverTypes.h"
#include "GSTN_WallMovementTypes.generated.h"

class UPrimitiveComponent;
struct FRollbackBlackboardSimWrapper;

/** How 2D move input is mapped onto the wall plane. */
UENUM(BlueprintType)
enum class EGSTN_WallInputMapping : uint8
{
	/**
	 * The stick drives the capsule's own frame: left/right = sideways along the wall, forward/back = up/down the wall.
	 * Independent of camera yaw and of where the capsule faces, so a held key keeps its meaning around every corner
	 * (BotW-style). With the camera in front of the character, "left" is the character's left (screen right).
	 */
	SurfaceRelative,

	/**
	 * Input is read relative to the camera (ControlRotation) and the camera's screen axes are projected onto the wall.
	 * More intuitive on ceilings and overhangs, can feel less stable when the camera swings around.
	 */
	CameraRelative,
};

/** How the capsule rotation reacts when the wall it is attached to (a dynamic movement base) rotates. */
UENUM(BlueprintType)
enum class EGSTN_WallBaseRotation : uint8
{
	/** The capsule rotates exactly with the base */
	Follow,
	/**
	 * Follows the base except for the roll around the wall normal: a wall spinning around an axis through its face
	 * (e.g. a turning wheel seen from the front) carries the character along without turning it upside down
	 */
	FollowExceptRoll,
	/** Moves with the base, but the orientation is left to the wall logic alone */
	Ignore,
};

// ------------------------------------------------------------------------------------------------
// Settings
// ------------------------------------------------------------------------------------------------

/**
 * Horizontal (sideways along the wall, perpendicular to gravity) movement on a wall. Same model as the
 * CharacterMovementComponent: Friction steers the velocity while there is input, braking (proportional + constant)
 * slows it down without input, and a stop speed snaps tiny residual speeds to zero.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallHorizontalMovement
{
	GENERATED_BODY()

	/** Target speed with full stick input (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 160.f;

	/** Acceleration toward the target speed while there is input (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 900.f;

	/**
	 * Friction while there is input (1/s): how fast a velocity going the "wrong" way is turned toward the input direction.
	 * High = snappy reversals, low = slippery, momentum carries on
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float Friction = 8.f;

	/** Braking without input, proportional to the speed (1/s). Never reaches zero on its own: see StopSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float BrakingFriction = 0.f;

	/** Constant braking without input, and when above the target speed (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float BrakingDeceleration = 1400.f;

	/** Absolute speed limit along this axis, whatever the source (momentum, entry, bases). 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalSpeed = 0.f;

	/** Without input, speeds below this snap to zero (consistent stops even with very low braking) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StopSpeed = 5.f;
};

/**
 * Vertical (along gravity projected on the wall) movement on a wall. Same model as the horizontal one, split up/down.
 * Exposed as is by modes with vertical input (UGSTN_SimpleWallMovementMode); other modes build it internally
 * (see UGSTN_WallMovementModeBase::GetVerticalMovement)
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallVerticalMovement
{
	GENERATED_BODY()

	/** Target speed with full stick input upward (cm/s). 0 = no climbing up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxUpSpeed = 160.f;

	/** Target speed with full stick input downward (cm/s). 0 = no climbing down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxDownSpeed = 160.f;

	/** Acceleration toward the target speed while there is input (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 900.f;

	/** Friction while there is input (1/s), see the horizontal one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float Friction = 8.f;

	/** Braking without input, proportional to the speed (1/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float BrakingFriction = 0.f;

	/** Constant braking without input, and when above the target speed (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float BrakingDeceleration = 1400.f;

	/** Absolute upward speed limit, whatever the source (entry momentum, bases). 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalUpSpeed = 0.f;

	/** Absolute downward speed limit, e.g. the max wall-slide speed. 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalDownSpeed = 0.f;

	/** Without input, speeds below this snap to zero (applied before gravity, so it never stops a slide) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StopSpeed = 5.f;
};

/**
 * Skid: a sharp brake when the stick is pushed against a fast horizontal movement (e.g. moving right, pushing left).
 * While skidding, acceleration toward the input is suspended and the character brakes hard; then it turns around.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallSkid
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bEnableSkid = true;

	/** Horizontal speed needed to start a skid (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s", EditCondition = "bEnableSkid"))
	float MinSpeed = 150.f;

	/** Minimum stick strength (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bEnableSkid"))
	float MinInputStrength = 0.5f;

	/**
	 * dot(input direction, horizontal velocity direction) must be at or below this to start: -1 = exactly opposite only,
	 * 0 = anything not pushing along the movement. A diagonal stick counts only for its opposing part
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "-1", ClampMax = "0", EditCondition = "bEnableSkid"))
	float InputDotThreshold = -0.5f;

	/** Hysteresis: an ongoing skid keeps going while the dot stays below InputDotThreshold + this */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bEnableSkid"))
	float ExitDotMargin = 0.25f;

	/** Braking while skidding (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s^2", EditCondition = "bEnableSkid"))
	float Deceleration = 3000.f;

	/** The skid ends below this horizontal speed, and normal acceleration takes over in the new direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s", EditCondition = "bEnableSkid"))
	float EndSpeed = 20.f;
};

/** What happens to the incoming velocity when the mode starts (e.g. grabbing a wall while falling) */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallEntryMomentum
{
	GENERATED_BODY()

	/** Fraction of the horizontal (along the wall) incoming speed kept */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float HorizontalScale = 1.f;

	/** Fraction of the vertical incoming speed kept (e.g. 0 = every grab starts still) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float VerticalScale = 1.f;

	/** Upward speed is clamped to this on entry. Negative = no clamp, 0 = no upward momentum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "cm/s"))
	float MaxUpSpeed = -1.f;

	/** Downward speed is clamped to this on entry. Negative = no clamp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "cm/s"))
	float MaxDownSpeed = -1.f;
};

/** Ways to let go of the wall on purpose. Each one can be enabled on its own */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallDetach
{
	GENERATED_BODY()

	/** Detach by holding the stick away from the wall for PullAwayDelay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bDetachByPullingAway = false;

	/**
	 * How much the (camera-relative, flattened) stick must point away from the wall: dot(input, horizontal wall normal).
	 * 1 = straight away only, 0.3 = any clearly outward direction
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bDetachByPullingAway"))
	float PullAwayInputDot = 0.5f;

	/** How long the stick must be held away (s). 0 = instant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "s", EditCondition = "bDetachByPullingAway"))
	float PullAwayDelay = 0.2f;

	/**
	 * Detach when the detach button is pressed: FGSTN_WallInputs::bWantsToDetach in the input collection, or
	 * (see bUseCrouchAsDetachButton) the CharacterMover crouch intent
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bDetachByButton = false;

	/**
	 * Without FGSTN_WallInputs in the input collection, use the CharacterMover crouch intent as the detach button.
	 * Works out of the box but is read from the component, so it is not predicted/replicated like a real input
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (EditCondition = "bDetachByButton"))
	bool bUseCrouchAsDetachButton = true;

	/** Mode to switch to when detaching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FName DetachModeName;
};

/** Provisional wall jump: jump pressed on the wall -> launch away from it into JumpModeName */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallJump
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bEnableWallJump = false;

	/** Speed along the wall normal (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s", EditCondition = "bEnableWallJump"))
	float AwaySpeed = 600.f;

	/** Speed along gravity-up (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s", EditCondition = "bEnableWallJump"))
	float UpSpeed = 700.f;

	/** Fraction of the current horizontal wall velocity kept */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", EditCondition = "bEnableWallJump"))
	float HorizontalMomentumScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (EditCondition = "bEnableWallJump"))
	FName JumpModeName;
};

/**
 * Which surfaces a wall mode (or the entry transition) accepts as a wall, on top of the angle test. The surface of a hit comes
 * from UGSTN_SurfaceUtils::Resolve (GSTNSurface plugin, traversal-surfaces.md §5.2): a UGSTN_SurfaceBehaviourComponent on the
 * collider's actor, a provider, or the physical material table. A surface without the behaviour is simply not a wall for this
 * mode: it still blocks the character, who slides along it or lands on it like on any geometry.
 *
 * Read-only configuration (no state, no cache): a re-simulated frame resolves the same surface. An empty query is the old
 * behaviour exactly: every hit passes and the surface is not even resolved.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallSurfaceFilter
{
	GENERATED_BODY()

	/**
	 * Behaviours (Surface.Behaviour.*) a surface must offer to count as a wall, e.g. WallSlide for the slide mode and WallClimb
	 * for the climb mode. Empty = no filter
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FGameplayTagQuery RequiredBehaviours;

	/**
	 * Surfaces nothing marked (no surface component, provider or material entry: they resolve to the plugin's default) pass
	 * without the query being evaluated, so level geometry that was never authored keeps working as before. Turn off to make
	 * only authored surfaces walls. Ignored while RequiredBehaviours is empty
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bUnknownSurfacesPass = true;

	/**
	 * More ways to pass: a surface is a wall if it matches RequiredBehaviours (when that is set) OR any of these. Empty by default, which is the old
	 * single-query behaviour exactly. The v2 modes fill it from their surface mode list (UGSTN_WallTraverseModeBase::GetWallCheckSettings) so that a
	 * climb mode riding on a wall also accepts the surfaces it can hand over to, without the probes dropping the wall before the switch. An empty query
	 * in this list matches every surface
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	TArray<FGameplayTagQuery> AlternativeBehaviours;

	bool IsEmpty() const { return RequiredBehaviours.IsEmpty() && AlternativeBehaviours.IsEmpty(); }
};

/**
 * Parameters of the wall queries (UGSTN_WallQueryUtils), built by the wall modes from their Surface Detection and
 * Adhesion settings. Same role as FFloorCheckSettings for the ground modes
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallCheckSettings
{
	GENERATED_BODY()

	/** Minimum angle between a surface normal and gravity-up for it to count as a wall (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "deg"))
	float MinSurfaceAngle = 40.f;

	/** Maximum angle between a surface normal and gravity-up (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "deg"))
	float MaxSurfaceAngle = 180.f;

	/** Desired gap between the capsule surface and the wall (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "cm"))
	float WallGap = 2.f;

	/** How far past WallGap the probes search for the wall (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "cm"))
	float WallProbeDistance = 25.f;

	/** Probe sphere radius as a fraction of the capsule radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float ProbeRadiusScale = 0.6f;

	/** Where the top/bottom probes sit along the capsule cylinder (0 = center, 1 = hemisphere centers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float ProbeVerticalSpread = 0.8f;

	/** Probe hits deviating more than this from the current wall normal are discarded (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "deg"))
	float MaxProbeDeviationAngle = 70.f;

	/** Max angle of an inside (concave) corner that can be transitioned onto (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "deg"))
	float MaxConcaveAngle = 120.f;

	/** Max angle of an outside (convex) corner that can be wrapped around (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "deg"))
	float MaxConvexAngle = 120.f;

	/** Lookahead along the movement direction, in seconds of travel (s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "s"))
	float LookaheadTime = 0.2f;

	/** Minimum lookahead distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ForceUnits = "cm"))
	float MinLookaheadDistance = 15.f;

	/** Which surfaces count as a wall besides the angle test. Default: all */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FGSTN_WallSurfaceFilter SurfaceFilter;

	/**
	 * Other pawns are never walls: their capsules are ignored by the contact, corner and wrap queries and refused by IsHitOnValidWall. Off for the
	 * legacy modes (unchanged behaviour); the v2 modes fill it from FGSTN_WallEdgeSettings::bIgnorePawns (wall-slide-v2.md W5, finding 15)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bIgnorePawns = false;

	/** Draw the probes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bDrawDebug = false;
};

// ------------------------------------------------------------------------------------------------
// Movement plane
// ------------------------------------------------------------------------------------------------

/**
 * The plane a wall move happens in: the filtered wall normal and its two movement axes. Vertical = gravity-up
 * projected on the wall (capsule-relative on ceilings, where gravity has no direction along the surface), and
 * Horizontal = Normal ^ Vertical. See UGSTN_WallMovementUtils::MakeMovementPlane
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallMovementPlane
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Mover|Wall")
	FVector Normal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Mover|Wall")
	FVector HorizontalAxis = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Mover|Wall")
	FVector VerticalAxis = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Mover|Wall")
	FVector GravityUp = FVector::UpVector;

	/** Capsule orientation the plane was built from (the basis on ceilings, and for surface-relative input) */
	UPROPERTY(BlueprintReadWrite, Category = "Mover|Wall")
	FQuat Orientation = FQuat::Identity;

	float GetHorizontalSpeed(const FVector& Velocity) const { return FVector::DotProduct(Velocity, HorizontalAxis); }
	float GetVerticalSpeed(const FVector& Velocity) const { return FVector::DotProduct(Velocity, VerticalAxis); }
	FVector MakeVelocity(float HorizontalSpeed, float VerticalSpeed) const { return HorizontalAxis * HorizontalSpeed + VerticalAxis * VerticalSpeed; }
};

// ------------------------------------------------------------------------------------------------
// Input and sync state
// ------------------------------------------------------------------------------------------------

/**
 * Optional input struct for wall modes. Add it to the input collection when producing input (e.g. in OnProduceInput)
 * to drive wall-specific actions through Mover's predicted, replicated input path.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallInputs : public FMoverDataStructBase
{
	GENERATED_BODY()

	/** Let go of the wall (see FGSTN_WallDetach::bDetachByButton) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Wall")
	bool bWantsToDetach = false;

	virtual FMoverDataStructBase* Clone() const override { return new FGSTN_WallInputs(*this); }
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_WallInputs> : public TStructOpsTypeTraitsBase2<FGSTN_WallInputs>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

/**
 * Wall state, carried in the Mover sync state so it is predicted, rolled back and replicated together with the rest
 * of the simulation. Movement modes must not keep per-frame state on the UObject itself.
 * It stays in the collection after leaving the wall: the "wall memory" fields are what entry transitions read.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallMovementSyncState : public FMoverDataStructBase
{
	GENERATED_BODY()

	/** Spatially and temporally filtered normal of the wall we are attached to. This is the "true" movement plane; the capsule orientation chases it. */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	FVector WallNormal = FVector::ZeroVector;

	/** Averaged contact point on the wall, used as rotation pivot */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	FVector WallContactPoint = FVector::ZeroVector;

	/** True if the last probe found a valid wall */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	bool bHasWall = false;

	/** Seconds since we last had a valid wall */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	float TimeWithoutWall = 0.f;

	/** True while skidding (hard brake against the movement). Meant for animation. Written by UGSTN_SkiddingWallMovementMode */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	bool bIsSkidding = false;

	/** Time the stick has been held away from the wall (detach by pulling away). Written by UGSTN_ControlledWallMovementMode */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	float PullAwayTime = 0.f;

	// ---- Wall memory: last wall touched, kept after leaving it (re-entry rules, height cap)

	/** True once the character has been on a wall (until the memory is reset) */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	bool bHasWallMemory = false;

	/** Filtered normal of the last wall */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	FVector LastWallNormal = FVector::ZeroVector;

	/** Height (location along gravity-up) on the last tick spent on a wall */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	float LastWallHeight = 0.f;

	/** Simulation time (ms) of the last tick spent on a wall */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	double LastWallTimeMs = 0.0;

	/**
	 * Simulation time (ms) of the last manual wall memory reset (FGSTN_ResetWallMemoryEffect / UGSTN_WallMovementLibrary::ResetWallMemory),
	 * negative if none. Walls left before it are forgotten. It lives here, not on the Mover blackboard, so it is predicted,
	 * rolled back and replicated with the rest of the wall state
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	double MemoryResetTimeMs = -1.0;

	/**
	 * Consecutive ticks the contact surface asked for another wall mode than the active one (UGSTN_WallTraverseModeBase, wall-slide-v2.md 6.3): the
	 * hysteresis of the in-place switch between slide and climb. In the sync state, not on the mode, so a re-simulated frame counts the same
	 * ticks. Reset on every attachment and on every switch. Sent only when non-zero; not compared by ShouldReconcile (a mode difference is reconciled
	 * through the mode name itself)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	uint8 SurfaceSwitchTicks = 0;

	virtual FMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

template<>
struct TStructOpsTypeTraits<FGSTN_WallMovementSyncState> : public TStructOpsTypeTraitsBase2<FGSTN_WallMovementSyncState>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

// ------------------------------------------------------------------------------------------------
// Simulation data (not reflected)
// ------------------------------------------------------------------------------------------------

/** Wall found by an entry transition, handed to the wall mode through the sim blackboard so it starts aligned to it */
struct FGSTN_WallEntryInfo
{
	FVector Normal = FVector::ZeroVector;
	FVector Point = FVector::ZeroVector;
};

/**
 * Dynamic wall the character rides, stored between ticks in a Mover rollback blackboard entry (GSTNWallBlackboard::BaseInfo,
 * InvalidatedOnRollback, one value per frame). An invalid Base means "not riding anything".
 */
struct FGSTN_WallBaseInfo
{
	TWeakObjectPtr<UPrimitiveComponent> Base;
	FName BoneName = NAME_None;
	FVector BaseLocation = FVector::ZeroVector;
	FQuat BaseRotation = FQuat::Identity;
	/** Character location in the base's frame */
	FVector LocalPosition = FVector::ZeroVector;
};

/** Result of a multi-probe wall query */
struct FGSTN_WallProbeResult
{
	FVector Normal = FVector::ZeroVector;
	FVector ContactPoint = FVector::ZeroVector;
	/** Component of the strongest along-wall sample: the wall's movement base candidate */
	TWeakObjectPtr<UPrimitiveComponent> Component;
	FName BoneName = NAME_None;
	bool bValid = false;
};

/** Inside corner met by a move: a blocking hit on another climbable surface the capsule fits on */
struct FGSTN_WallCornerHit
{
	FVector Normal = FVector::ZeroVector;
	FVector Point = FVector::ZeroVector;

	bool IsValid() const { return !Normal.IsNearlyZero(); }
};

/**
 * Working state of one wall simulation tick, passed through the steps of UGSTN_WallMovementModeBase::SimulationTick.
 * Starts from FGSTN_WallMovementSyncState and is written back into it at the end of the tick
 */
struct FGSTN_WallTickState
{
	/** Filtered wall normal: the movement plane */
	FVector WallNormal = FVector::ZeroVector;
	FVector WallContactPoint = FVector::ZeroVector;
	bool bHasWall = false;
	float TimeWithoutWall = 0.f;
	float PullAwayTime = 0.f;

	/** Velocity along the wall, transported around corners */
	FVector Velocity = FVector::ZeroVector;
	/** Current capsule orientation */
	FQuat Orientation = FQuat::Identity;
	FVector GravityUp = FVector::UpVector;

	/** Last successful probe: its component is the movement base candidate for next tick */
	FGSTN_WallProbeResult LastProbe;

	/** The contact of this tick's adhesion sweep (a wall found by the sweep itself, not a wrap or a corner), for the surface-driven mode choice */
	FHitResult Contact;
	bool bHasContact = false;

	/** Held in place from outside this tick (IsMovementLocked of the v2 modes): no exits, no timers. Tick-local, not part of the sync state */
	bool bMovementLocked = false;

	/** Ticks in a row the surface asked for another mode (FGSTN_WallMovementSyncState::SurfaceSwitchTicks) */
	uint8 SurfaceSwitchTicks = 0;

	/** Mode switch decided this tick, with the simulation time it leaves unused */
	FName NextModeName = NAME_None;
	float UnusedSeconds = 0.f;

	bool IsLeaving() const { return !NextModeName.IsNone(); }

	void LeaveTo(FName ModeName, float InUnusedSeconds)
	{
		NextModeName = ModeName;
		UnusedSeconds = InUnusedSeconds;
	}
};

namespace GSTNWallBlackboard
{
	/**
	 * FGSTN_WallEntryInfo written by entry transitions (Trigger), consumed by the mode's Activate (which overwrites it with an
	 * empty one). A zero normal means "none".
	 */
	extern GASP_GIUSTINO2_API const FName EntryInfo;
	/** FGSTN_WallBaseInfo of the dynamic wall being ridden (invalid Base = none) */
	extern GASP_GIUSTINO2_API const FName BaseInfo;

	/**
	 * Both entries are Mover rollback blackboard entries (not the plain SimBlackboard, which is never rewound): one value per
	 * frame for as many frames as a rollback can rewind (Network Prediction's 64 frame buffer), invalidated on rollback.
	 * Idempotent: modes create them in OnRegistered, the entry transition on first use.
	 */
	GASP_GIUSTINO2_API void EnsureEntries(FRollbackBlackboardSimWrapper& Blackboard);
}
