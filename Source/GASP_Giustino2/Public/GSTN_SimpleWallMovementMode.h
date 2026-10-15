// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MovementModeBase.h"
#include "MoverTypes.h"
#include "GSTN_SimpleWallMovementMode.generated.h"

struct FCharacterDefaultInputs;
struct FMovementRecord;
struct FMovingComponentSet;

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

/** Vertical (along gravity projected on the wall) movement on a wall. Same model as the horizontal one, split up/down */
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

	/** Braking without input, proportional to the speed (1/s). On a WallSlide this acts as wall friction against gravity */
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

	/** True while skidding (hard brake against the movement). Meant for animation */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	bool bIsSkidding = false;

	/** Time the stick has been held away from the wall (detach by pulling away) */
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

/** Wall found by an entry transition, handed to the wall mode through the sim blackboard so it starts aligned to it */
struct FGSTN_WallEntryInfo
{
	FVector Normal = FVector::ZeroVector;
	FVector Point = FVector::ZeroVector;
};

/** Dynamic wall the character rides, stored on the sim blackboard between ticks */
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

namespace GSTNWallBlackboard
{
	/** FGSTN_WallEntryInfo written by entry transitions, consumed by the mode's Activate */
	extern GASP_GIUSTINO2_API const FName EntryInfo;
	/** FGSTN_WallBaseInfo of the dynamic wall being ridden */
	extern GASP_GIUSTINO2_API const FName BaseInfo;
	/** double: sim time (ms) of the last manual wall memory reset. Walls left before it are forgotten (no height cap) */
	extern GASP_GIUSTINO2_API const FName MemoryResetTimeMs;
}

/**
 * Basic wall movement: free, accelerated movement on arbitrary surfaces (walls, overhangs, ceilings),
 * wrapping around convex and concave corners. Base for Climbing / WallSliding modes. Works with any gravity
 * direction (the capsule keeps upright relative to it) and on moving/rotating walls (dynamic movement bases).
 *
 * Three separate quantities, each with its own settings category:
 *  - Raw probe normal   ("Surface Detection"): what the probes measure this substep. Noisy, can jump at edges
 *  - Filtered normal    ("Normal Filter"): the wall normal kept in the sync state. It is the MOVEMENT PLANE: velocity,
 *                       input mapping, adhesion and probe directions all use it
 *  - Capsule rotation   ("Orientation"): purely visual/collision pose. It CHASES the filtered normal with its own smoothing,
 *                       but never feeds back into the movement plane
 *  Position against the wall is a fourth, independent concern ("Adhesion").
 *
 * Velocity is split on the wall plane into a horizontal and a vertical axis relative to gravity (capsule axes on
 * ceilings, where gravity has no direction along the surface), each with its own speed/friction/braking/terminal speed.
 *
 * Per tick: ride the movement base, handle wall jump / detach, then per substep:
 *  1. Sweep the capsule along the wall tangent (slide on blocking hits; a blocking wall the capsule fits on becomes a
 *     concave-corner candidate; moving down into a floor switches to FloorModeName)
 *  2. Probe the wall with several sphere sweeps (center/top/bottom + lookahead) and blend their normals (spatial filter).
 *     Hits on edges leading onto non-wall faces (roofs, floors) are discarded
 *  3. If nothing is found, run a wrap probe around the convex edge we just passed. No wall for NoWallGraceTime -> NoWallModeName
 *  4. Filter the normal: soft deadzone (surface noise), rate limit (popping samples). Frozen while at rest
 *  5. Build the target capsule orientation by parallel-transporting the current frame onto the filtered normal, plus upright/heading bias
 *  6. Smooth toward it (time + distance based) and apply the rotation around the wall contact point, validated by an overlap test
 *  7. Adhere to the wall at a fixed gap (instant push-out, rate-limited pull-in, real penetration resolution)
 *  8. Parallel-transport the velocity onto the new plane (speed is preserved around corners)
 */
UCLASS(Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_SimpleWallMovementMode : public UGSTN_MovementModeBase
{
	GENERATED_BODY()

public:
	UGSTN_SimpleWallMovementMode(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	virtual void OnRegistered(const FName ModeName, const FMoverSimContext& SimContext) override;
	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	// ---------------------------------------------------------------- Movement
	// Velocity inside the movement plane (the filtered wall normal). None of these touch the normal or the capsule rotation.

	/** Sideways along the wall (perpendicular to gravity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallHorizontalMovement Horizontal;

	/** Up/down along the wall (gravity projected on the wall) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallVerticalMovement Vertical;

	/** Hard brake when pushing against a fast horizontal movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallSkid Skid;

	/** How the stick is turned into a direction on the wall. See EGSTN_WallInputMapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	EGSTN_WallInputMapping InputMapping = EGSTN_WallInputMapping::SurfaceRelative;

	/** Moves longer than this are split in substeps, so corners are never skipped at speed. 0 = half the capsule radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxSubstepDistance = 0.f;

	/** Upper bound on substeps per tick (cost cap). If hit, substeps get longer than MaxSubstepDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxSubsteps = 4;

	// ---------------------------------------------------------------- Entry / Exit

	/** What happens to the incoming velocity when the mode starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FGSTN_WallEntryMomentum EntryMomentum;

	/** Ways to let go of the wall on purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FGSTN_WallDetach Detach;

	/** Provisional wall jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FGSTN_WallJump WallJump;

	/** Mode to switch to after losing the wall for longer than NoWallGraceTime. None = stay in this mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FName NoWallModeName;

	/** How long the wall can be missing (e.g. while wrapping an edge) before leaving the mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit", meta = (ClampMin = "0", ForceUnits = "s"))
	float NoWallGraceTime = 0.15f;

	/** Mode to switch to when moving down into a floor (a surface below MinSurfaceAngle). None = stay in this mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FName FloorModeName;

	// ---------------------------------------------------------------- Movement Bases

	/** Ride moving/rotating walls (dynamic movement bases) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement Bases")
	bool bUseMovementBases = true;

	/** [Rotation] How the capsule follows a rotating wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement Bases", meta = (EditCondition = "bUseMovementBases"))
	EGSTN_WallBaseRotation BaseRotation = EGSTN_WallBaseRotation::FollowExceptRoll;

	/** When leaving the wall, add the wall's own velocity at the contact point (momentum from a moving platform) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement Bases", meta = (EditCondition = "bUseMovementBases"))
	bool bAddBaseVelocityOnExit = true;

	// ---------------------------------------------------------------- Surface Detection
	// What the probes accept and where they look. Output: the RAW PROBE NORMAL (and contact point) of this substep.
	// These shape which wall the character is on and how corners are sensed; they do not smooth anything by themselves.

	/**
	 * [Raw normal] Minimum angle between a surface normal and gravity-up for it to count as a wall. Below this it's a floor:
	 * probes ignore it, and moving down into it switches to FloorModeName
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinSurfaceAngle = 40.f;

	/** [Raw normal] Maximum angle between a surface normal and gravity-up. 90 = vertical walls only, 180 = ceilings allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxSurfaceAngle = 180.f;

	/**
	 * [Raw normal] How far past WallGap the probes search for the wall. Too short and small bumps/recesses lose the wall
	 * (-> NoWallModeName); too long and the probes start seeing nearby geometry that isn't the current wall
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallProbeDistance = 25.f;

	/**
	 * [Raw normal] Probe sphere radius as a fraction of the capsule radius. A sphere returns a "rounded" normal on edges, so
	 * bigger = smoother transitions and small crevices ignored, but it also grazes more neighbouring geometry (props, corners)
	 * and that can pull the normal sideways. 0.6 is a good default
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ProbeRadiusScale = 0.6f;

	/** [Raw normal] Where the top/bottom probes sit along the capsule cylinder (0 = all at the center, 1 = hemisphere centers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "1"))
	float ProbeVerticalSpread = 0.8f;

	/**
	 * [Raw normal] Probe hits deviating more than this from the current filtered normal are discarded. Weights fade to zero
	 * toward this angle, so samples never pop in or out
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxProbeDeviationAngle = 70.f;

	/** [Raw normal] Max angle of an inside (concave) corner that can be transitioned onto. The capsule must also fit on the new wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxConcaveAngle = 120.f;

	/** [Raw normal] Max angle of an outside (convex) corner that can be wrapped around */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxConvexAngle = 120.f;

	/**
	 * [Raw normal] Lookahead along the movement direction, in seconds of travel. An inside corner ahead is added to the raw
	 * normal with a weight that grows as it gets closer, so the movement plane (and then the capsule) starts turning BEFORE
	 * touching it. This anticipates WHERE the normal goes; how smoothly the capsule follows is set in Orientation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ForceUnits = "s"))
	float LookaheadTime = 0.2f;

	/** [Raw normal] Minimum lookahead distance, so corners are anticipated even when moving slowly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinLookaheadDistance = 15.f;

	// ---------------------------------------------------------------- Normal Filter
	// Raw probe normal -> FILTERED NORMAL (sync state). This is the movement plane: velocity, input mapping, adhesion
	// and next substep's probe directions use it. The capsule rotation then chases it (see Orientation).

	/**
	 * [Filtered normal] Soft deadzone: raw changes smaller than this are progressively ignored (surface noise, bumpy walls),
	 * bigger ones pass through untouched
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Normal Filter", meta = (ClampMin = "0", ClampMax = "30", ForceUnits = "deg"))
	float NormalDeadzoneAngle = 4.f;

	/**
	 * [Filtered normal] Max angular speed of the movement plane. Stops a probe sample appearing/disappearing from snapping it
	 * in a single step. Keep it well above how fast corners are taken (90 deg in ~0.2 s = 450 deg/s). 0 = unlimited
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Normal Filter", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float MaxNormalChangeRate = 540.f;

	// ---------------------------------------------------------------- Adhesion
	// Distance from the wall along the filtered normal. Position only: no effect on the normal or the rotation.

	/** [Position] Desired gap between the capsule surface and the wall. Also part of the probes' reach */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Adhesion", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallGap = 2.f;

	/** [Position] Max pull-in speed toward the wall (bumpy walls don't pop the capsule in). Push-out is always instant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Adhesion", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxAdhesionSpeed = 400.f;

	// ---------------------------------------------------------------- Orientation
	// Capsule rotation chasing the filtered normal. Rotation only: none of these change the movement plane, so they can be
	// tuned for looks/animation without affecting where the character goes.

	/** [Rotation] Time-based smoothing toward the target orientation (1/s). Higher = snappier. Acts even when standing still */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0"))
	float RotationSharpness = 10.f;

	/**
	 * [Rotation] Distance-based smoothing: distance travelled to close ~63% of the remaining angle. Keeps corner rotation
	 * consistent at any speed. The faster of time/distance smoothing wins
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "1", ForceUnits = "cm"))
	float RotationSmoothingDistance = 20.f;

	/** [Rotation] Hard cap on the capsule angular speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float MaxRotationRate = 720.f;

	/**
	 * [Rotation] How fast the capsule up re-aligns with gravity-up on walls (roll around the wall normal, 1/s). Works with any
	 * gravity direction. Fades out to 0 on ceilings
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0"))
	float UprightSharpness = 6.f;

	/** [Rotation] On ceilings (where "up" is undefined) turn the capsule up toward the movement direction, like a walking heading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation")
	bool bAlignHeadingToMovementOnCeilings = true;

	/** [Rotation] How fast the heading follows the movement direction on ceilings (1/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", EditCondition = "bAlignHeadingToMovementOnCeilings"))
	float CeilingHeadingSharpness = 5.f;

	// ---------------------------------------------------------------- Debug

	/** Draws the capsule (green = on wall, red = no wall), filtered normal (cyan), capsule up (blue), velocity (yellow), probes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Debug")
	bool bDrawDebug = false;

protected:
	// ---------------------------------------------------------------- Velocity model (override points)

	/** Maps character input to a wall-tangent intent vector (length <= 1) */
	virtual FVector MapInputToWall(const FCharacterDefaultInputs* Inputs, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp) const;

	/**
	 * Extra acceleration along the vertical wall axis (positive = up), e.g. gravity sliding.
	 * @param VerticalSpeed        current speed along the vertical axis (positive = up)
	 * @param GravityAlongWall     gravity acceleration projected on the vertical axis (negative = pulls down)
	 * Base: none, the character clings to the wall
	 */
	virtual float GetVerticalForce(float VerticalSpeed, float GravityAlongWall) const { return 0.f; }

	/** Horizontal/vertical axes of the wall plane: gravity-relative on walls, capsule-relative on ceilings */
	void GetWallAxes(const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp, FVector& OutHorizontal, FVector& OutVertical) const;

	/** Whether the move starting from StartState is a skid (deterministic: used by both GenerateMove and SimulationTick) */
	bool ComputeSkid(bool bWasSkidding, const FVector& Intent, float HorizontalSpeed, const FVector& HorizontalAxis) const;

	/** One axis of the CMC-style velocity model */
	static float UpdateAxisSpeed(float Speed, float TargetSpeed, float Acceleration, float Friction, float BrakingFriction,
		float BrakingDeceleration, float StopSpeed, float DeltaSeconds);

	// ---------------------------------------------------------------- Sensing / pose

	/** Whether a surface with this normal is a wall we can stay on */
	virtual bool IsValidWallNormal(const FVector& Normal, const FVector& GravityUp) const;

	/**
	 * Whether a sweep hit lies on climbable geometry. On edges a sphere/capsule sweep returns a "rounded" normal blended between
	 * the two faces: that is only trusted if both faces are walls, so the lip of a floor or roof can't tilt the capsule toward it.
	 */
	bool IsHitOnValidWall(const UWorld* World, const FHitResult& Hit, const FVector& GravityUp, ECollisionChannel Channel,
		const FCollisionQueryParams& QueryParams, const FCollisionResponseParams& ResponseParams) const;

	/** Builds the capsule's target orientation for a wall normal */
	virtual FQuat ComputeTargetOrientation(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const;

	/** Multi-probe wall query around the capsule */
	bool ProbeWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& Velocity,
		const FVector& ConcaveHitNormal, const FVector& ConcaveHitPoint, const FVector& GravityUp, float DeltaSeconds, FGSTN_WallProbeResult& OutResult) const;

	/** Probe around an outside edge we just moved past */
	bool ProbeConvexWrap(const FMovingComponentSet& MovingComps, const FVector& WallNormal, const FVector& MoveDelta, const FVector& GravityUp, FGSTN_WallProbeResult& OutResult) const;

	/** Rotates the capsule around Pivot, backing off if the new pose would overlap geometry. Returns the orientation actually applied */
	FQuat ApplyRotationAroundPivot(const FMovingComponentSet& MovingComps, const FQuat& FromQuat, const FQuat& ToQuat, const FVector& Pivot, FMovementRecord& MoveRecord) const;

	/** Keeps the capsule at WallGap from the wall */
	void AdhereToWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp, float DeltaSeconds, FMovementRecord& MoveRecord) const;

	/** Moves/rotates the capsule, the wall normal and the velocity with the dynamic wall it is riding */
	void ApplyMovementBase(const FMovingComponentSet& MovingComps, FVector& InOutWallNormal, FVector& InOutVelocity, FMovementRecord& MoveRecord) const;

	/** Stores (or clears) the dynamic wall to ride next tick */
	void CaptureMovementBase(const FMovingComponentSet& MovingComps, const FGSTN_WallProbeResult& Probe, bool bOnWall) const;

	/** Whether the detach button is held (FGSTN_WallInputs, or the CharacterMover crouch intent as a fallback) */
	bool IsDetachButtonPressed(const FMoverTickStartData& StartState) const;

	/** Tangent basis of the wall plane, aligned with the capsule's heading */
	static void GetWallTangentBasis(const FQuat& Orientation, const FVector& WallNormal, FVector& OutTangentUp, FVector& OutTangentRight);

	void GetCapsuleSize(const UPrimitiveComponent* Primitive, float& OutRadius, float& OutHalfHeight) const;

	FName RegisteredModeName = NAME_None;

private:
	// ---- Legacy settings (before the horizontal/vertical split). Not editable: only loaded from old assets and migrated in PostLoad
	UPROPERTY()
	float MaxSpeed = 160.f;
	UPROPERTY()
	float Acceleration = 900.f;
	UPROPERTY()
	float Deceleration = 1400.f;
	UPROPERTY()
	float UpSpeedScale = 1.f;
	UPROPERTY()
	float DownSpeedScale = 1.f;
	UPROPERTY()
	float LateralSpeedScale = 1.f;
};
