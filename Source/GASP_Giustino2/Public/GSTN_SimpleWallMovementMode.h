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

/**
 * Wall-attachment state, carried in the Mover sync state so it is predicted, rolled back and replicated
 * together with the rest of the simulation. Movement modes must not keep per-frame state on the UObject itself.
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

	/** Seconds since we last had a valid wall. Intended for future transitions (e.g. drop to Falling after a grace time). */
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Wall")
	float TimeWithoutWall = 0.f;

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

/** Result of a multi-probe wall query */
struct FGSTN_WallProbeResult
{
	FVector Normal = FVector::ZeroVector;
	FVector ContactPoint = FVector::ZeroVector;
	bool bValid = false;
};

/**
 * Basic wall movement: free, accelerated movement on arbitrary surfaces (walls, overhangs, ceilings),
 * wrapping around convex and concave corners. Meant to be the base for Climbing / WallSliding modes.
 *
 * Three separate quantities, each with its own settings category:
 *  - Raw probe normal   ("Surface Detection"): what the probes measure this substep. Noisy, can jump at edges
 *  - Filtered normal    ("Normal Filter"): the wall normal kept in the sync state. It is the MOVEMENT PLANE: velocity,
 *                       input mapping, adhesion and probe directions all use it
 *  - Capsule rotation   ("Orientation"): purely visual/collision pose. It CHASES the filtered normal with its own smoothing,
 *                       but never feeds back into the movement plane
 *  Position against the wall is a fourth, independent concern ("Adhesion").
 *
 * Per substep:
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

	virtual void OnRegistered(const FName ModeName, const FMoverSimContext& SimContext) override;
	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	// ---------------------------------------------------------------- Movement
	// Velocity inside the movement plane (the filtered wall normal). None of these touch the normal or the capsule rotation.

	/** Max speed along the wall (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 160.f;

	/** Acceleration toward the input velocity (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 900.f;

	/** Deceleration with no input, when over max speed, or when reversing (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Deceleration = 1400.f;

	/** Speed scale when moving "up" in the capsule frame (stick forward). 0 = no climbing up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0"))
	float UpSpeedScale = 1.f;

	/** Speed scale when moving "down" in the capsule frame (stick back). 0 = no climbing down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0"))
	float DownSpeedScale = 1.f;

	/** Speed scale when moving sideways (stick left/right) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0"))
	float LateralSpeedScale = 1.f;

	/** How the stick is turned into a direction on the wall. See EGSTN_WallInputMapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	EGSTN_WallInputMapping InputMapping = EGSTN_WallInputMapping::SurfaceRelative;

	/** Moves longer than this are split in substeps, so corners are never skipped at speed. 0 = half the capsule radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxSubstepDistance = 0.f;

	/** Upper bound on substeps per tick (cost cap). If hit, substeps get longer than MaxSubstepDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxSubsteps = 4;

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

	// ---------------------------------------------------------------- Transitions

	/** Mode to switch to after losing the wall for longer than NoWallGraceTime. None = stay in this mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Transitions")
	FName NoWallModeName;

	/** How long the wall can be missing (e.g. while wrapping an edge) before leaving the mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Transitions", meta = (ClampMin = "0", ForceUnits = "s"))
	float NoWallGraceTime = 0.15f;

	/** Mode to switch to when moving down into a floor (a surface below MinSurfaceAngle). None = stay in this mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Transitions")
	FName FloorModeName;

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

	/** [Rotation] How fast the capsule up re-aligns with gravity-up on walls (roll around the wall normal, 1/s). Fades out to 0 on ceilings */
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
	/** Maps character input to a wall-tangent intent vector (length <= 1) */
	virtual FVector MapInputToWall(const FCharacterDefaultInputs* Inputs, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp) const;

	/** Turns a wall-tangent intent into a target velocity. Override to add e.g. gravity sliding or stamina-based speed */
	virtual FVector ComputeTargetVelocity(const FVector& WallIntent, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp) const;

	/** Accelerates PriorVelocity toward TargetVelocity in the wall plane */
	virtual FVector ComputeWallVelocity(const FVector& PriorVelocity, const FVector& TargetVelocity, float DeltaSeconds) const;

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

	/** Tangent basis of the wall plane, aligned with the capsule's heading */
	static void GetWallTangentBasis(const FQuat& Orientation, const FVector& WallNormal, FVector& OutTangentUp, FVector& OutTangentRight);

	void GetCapsuleSize(const UPrimitiveComponent* Primitive, float& OutRadius, float& OutHalfHeight) const;

	FName RegisteredModeName = NAME_None;
};
