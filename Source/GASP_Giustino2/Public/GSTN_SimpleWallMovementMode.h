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
	 * "Push toward the wall = go up the wall". Input is read relative to the capsule's heading on the wall.
	 * Continuous across every corner (BotW-style), but on ceilings "forward" keeps meaning "onward from the wall you came from".
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
 * Per substep:
 *  1. Sweep the capsule along the wall tangent (slide on blocking hits; a blocking valid wall becomes a concave-corner candidate)
 *  2. Probe the wall with several sphere sweeps (center/top/bottom + lookahead) and blend their normals (spatial filter)
 *  3. If nothing is found, run a wrap probe around the convex edge we just passed
 *  4. Soft-deadzone the new normal against the previous one (rejects surface noise)
 *  5. Build the target capsule orientation by parallel-transporting the current frame onto the new normal, plus upright/heading bias
 *  6. Smooth toward it (time + distance based) and apply the rotation around the wall contact point, validated by an overlap test
 *  7. Adhere to the wall at a fixed gap (instant push-out, rate-limited pull-in)
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

	/** Max speed along the wall (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 160.f;

	/** Acceleration toward the input velocity (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 900.f;

	/** Deceleration with no input, when over max speed, or when reversing (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Deceleration = 1400.f;

	/** Speed scale when moving "up" in the capsule frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0"))
	float UpSpeedScale = 1.f;

	/** Speed scale when moving "down" in the capsule frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0"))
	float DownSpeedScale = 1.f;

	/** Speed scale when moving sideways */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0"))
	float LateralSpeedScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	EGSTN_WallInputMapping InputMapping = EGSTN_WallInputMapping::SurfaceRelative;

	/** Moves longer than this are split in substeps, so corners are never skipped at speed. 0 = half the capsule radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxSubstepDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxSubsteps = 4;

	// ---------------------------------------------------------------- Surface

	/** Minimum angle between the surface normal and gravity-up to count as a wall. Below this it's a floor, not a wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinSurfaceAngle = 40.f;

	/** Maximum angle between the surface normal and gravity-up. 90 = vertical walls only, 180 = ceilings allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxSurfaceAngle = 180.f;

	/** Desired gap between the capsule surface and the wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallGap = 2.f;

	/** How far past the desired gap the wall is searched for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallProbeDistance = 25.f;

	/** Probe sphere radius as a fraction of the capsule radius. Bigger = smoother (ignores smaller crevices) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ProbeRadiusScale = 0.6f;

	/** Where the top/bottom probes sit along the capsule cylinder (0 = center, 1 = hemisphere centers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ClampMax = "1"))
	float ProbeVerticalSpread = 0.8f;

	/** Probe hits deviating more than this from the current wall normal are discarded (outlier rejection) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxProbeDeviationAngle = 70.f;

	/** Max angle of an inside (concave) corner that can be transitioned onto */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxConcaveAngle = 120.f;

	/** Max angle of an outside (convex) corner that can be wrapped around */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxConvexAngle = 120.f;

	/** Lookahead along the movement direction, in seconds of travel. Lets the capsule start turning before reaching an inside corner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ForceUnits = "s"))
	float LookaheadTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinLookaheadDistance = 15.f;

	/** Max pull-in speed toward the wall. Push-out is always instant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxAdhesionSpeed = 400.f;

	// ---------------------------------------------------------------- Orientation

	/** Normal changes smaller than this are progressively ignored (soft deadzone against surface noise) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", ClampMax = "30", ForceUnits = "deg"))
	float NormalDeadzoneAngle = 4.f;

	/** Time-based orientation smoothing (1/s). Higher = snappier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0"))
	float RotationSharpness = 10.f;

	/** Distance-based orientation smoothing: distance travelled to close ~63% of the gap. Keeps corner rotation consistent at any speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "1", ForceUnits = "cm"))
	float RotationSmoothingDistance = 20.f;

	/** Hard cap on the capsule angular speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float MaxRotationRate = 720.f;

	/** How fast the capsule up re-aligns with gravity-up on walls (1/s). Fades out to 0 on ceilings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0"))
	float UprightSharpness = 6.f;

	/** On ceilings (where "up" is undefined) turn the capsule up toward the movement direction, like a walking heading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation")
	bool bAlignHeadingToMovementOnCeilings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", EditCondition = "bAlignHeadingToMovementOnCeilings"))
	float CeilingHeadingSharpness = 5.f;

	// ---------------------------------------------------------------- Debug

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
