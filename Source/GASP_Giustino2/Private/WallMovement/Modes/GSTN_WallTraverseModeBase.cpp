// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_WallTraverseModeBase.h"

#include "Components/PrimitiveComponent.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallTraverseModeBase)

UGSTN_WallTraverseModeBase::UGSTN_WallTraverseModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Detach and wall jump are the controlled mode's; subclasses enable what they offer
}

// ------------------------------------------------------------------------------------------------
// Entry
// ------------------------------------------------------------------------------------------------

void UGSTN_WallTraverseModeBase::Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	// The velocity the character arrives with, before the legacy entry momentum (EntryMomentum, per-axis scales) touches it
	FVector InVelocity = FVector::ZeroVector;
	if (OutSyncState)
	{
		if (const FMoverDefaultSyncState* DefaultSync = OutSyncState->SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			InVelocity = DefaultSync->GetVelocity_WorldSpace();
		}
	}

	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);
	if (!OutSyncState)
	{
		return;
	}

	const FGSTN_WallMovementSyncState* WallState = OutSyncState->SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();
	FMoverDefaultSyncState* DefaultSync = OutSyncState->SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (!WallState || !DefaultSync)
	{
		return;
	}

	// Momentum in (wall-slide-v2.md 6.2): the arrival velocity turned onto the plane, nothing zeroed. The capsule is put upright
	// with the facing yaw at once: it never tilts on a wall, so a pose inherited from another mode is straightened here, not smoothed
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FVector GravityUp = MoverComp ? MoverComp->GetUpDirection() : FVector::UpVector;
	const FVector NewVelocity = ComputeEntryVelocity(InVelocity, WallState->WallNormal);
	const FQuat Facing = ComputeFacingQuat(DefaultSync->GetOrientation_WorldSpace().Quaternion(), WallState->WallNormal, GravityUp);
	DefaultSync->SetTransforms_WorldSpace(DefaultSync->GetLocation_WorldSpace(), Facing.Rotator(), NewVelocity, DefaultSync->GetAngularVelocityDegrees_WorldSpace(), nullptr);

	GSTN_WALL_LOG(1, TEXT("ACTIVATE v2 from %s inVel=%s -> vel=%s"), *PrevModeName.ToString(), *GSTNWallDebug::Vec(InVelocity), *GSTNWallDebug::Vec(NewVelocity));
}

FVector UGSTN_WallTraverseModeBase::ComputeEntryVelocity(const FVector& InVelocity, const FVector& WallNormal) const
{
	return UGSTN_WallControlUtils::ProjectEntryVelocity(FGSTN_WallSlideControlSettings(), InVelocity, WallNormal);
}

FQuat UGSTN_WallTraverseModeBase::ComputeFacingQuat(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp)
{
	const FVector Up = GravityUp.GetSafeNormal();
	FVector Forward = FVector::VectorPlaneProject(-WallNormal, Up).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		// A floor or a ceiling: no heading to read from the wall, keep the one the capsule has
		Forward = FVector::VectorPlaneProject(CurrentQuat.GetForwardVector(), Up).GetSafeNormal();
	}
	if (Forward.IsNearlyZero())
	{
		return FRotationMatrix::MakeFromZ(Up).ToQuat();
	}
	return FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
}

// ------------------------------------------------------------------------------------------------
// GenerateMove: the control model builds the proposed velocity
// ------------------------------------------------------------------------------------------------

void UGSTN_WallTraverseModeBase::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FGSTN_WallMovementPlane Plane = MakeStartMovementPlane(StartState, StartingSyncState->GetOrientation_WorldSpace().Quaternion());
	const FVector WallIntent = ComputeWallIntent(StartState, Plane);

	OutProposedMove.LinearVelocity = ComputeWallVelocity(StartState, Plane, WallIntent, DeltaSeconds);
	OutProposedMove.DirectionIntent = WallIntent;
	OutProposedMove.bHasDirIntent = !WallIntent.IsNearlyZero();
	// The facing is presentation, resolved in the tick; the movement never asks for a rotation
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
}

FVector UGSTN_WallTraverseModeBase::ComputeWallVelocity(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float DeltaSeconds) const
{
	return GetPriorVelocityOnPlane(StartState, Plane);
}

FVector UGSTN_WallTraverseModeBase::GetWorldMoveIntent(const FMoverTickStartData& StartState, const FVector& GravityUp) const
{
	const FCharacterDefaultInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!Inputs)
	{
		return FVector::ZeroVector;
	}
	FVector MoveIntent = UMovementUtils::ComputeDirectionIntent(Inputs->GetMoveInput_WorldSpace(), Inputs->GetMoveInputType(), FMath::Max(Horizontal.MaxSpeed, 1.f));
#if GSTN_WALL_DEBUG
	GSTNWallDebug::OverrideMoveInput(Inputs->ControlRotation, GravityUp, MoveIntent);
#endif
	return MoveIntent.GetClampedToMaxSize(1.f);
}

FVector UGSTN_WallTraverseModeBase::GetPriorVelocityOnPlane(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane) const
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	return Sync ? FVector::VectorPlaneProject(Sync->GetVelocity_WorldSpace(), Plane.Normal) : FVector::ZeroVector;
}

// ------------------------------------------------------------------------------------------------
// Edge hooks
// ------------------------------------------------------------------------------------------------

float UGSTN_WallTraverseModeBase::GetEdgeReferenceSpeed() const
{
	return FGSTN_WallSlideControlSettings().TerminalDownSpeed;
}

FGSTN_WallAheadResult UGSTN_WallTraverseModeBase::ProbeAhead(const FMovingComponentSet& MovingComps, const FGSTN_WallTickState& State, const FGSTN_WallCheckSettings& CheckSettings) const
{
	return UGSTN_WallEdgeUtils::ProbeAhead(MovingComps, Edges, CheckSettings, State.WallNormal, State.GravityUp, State.Velocity);
}

EGSTN_WallEdgeKind UGSTN_WallTraverseModeBase::ClassifyAhead(const FGSTN_WallAheadResult& Ahead) const
{
	return Ahead.Kind;
}

float UGSTN_WallTraverseModeBase::ComputeEdgeSpeedCap(const FGSTN_WallAheadResult& Ahead) const
{
	// Only a real corner slows the character (the owner's rule); the approach curve makes it a brake that releases by itself
	const float CornerSpeed = UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, Ahead.Kind, Ahead.Angle, GetEdgeReferenceSpeed());
	if (CornerSpeed >= TNumericLimits<float>::Max())
	{
		return CornerSpeed;
	}
	return UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(Edges, CornerSpeed, Ahead.Distance);
}

bool UGSTN_WallTraverseModeBase::TryStepOverBump(const FMovingComponentSet& MovingComps, const FHitResult& Hit, const FVector& MoveDelta, const FGSTN_WallAheadResult& Ahead,
	float DeltaSeconds, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const
{
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent || Edges.MaxStepHeight <= 0.f)
	{
		return false;
	}
	// An inside corner (or a surface that cannot be turned onto) is ahead: what blocks is that, not a bump
	if (Ahead.Kind == EGSTN_WallEdgeKind::Concave || Ahead.Kind == EGSTN_WallEdgeKind::Blocked)
	{
		return false;
	}
	const FVector Dir = FVector::VectorPlaneProject(MoveDelta, InOutState.WallNormal).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return false;
	}

	// Size what blocks. Anything but a low feature (taller, a ramp, geometry around the probe) is for the corner / floor logic
	const FGSTN_WallCheckSettings CheckSettings = GetWallCheckSettings();
	const FGSTN_WallAheadResult Feature = UGSTN_WallEdgeUtils::ProbeBumpAt(MovingComps, Edges, CheckSettings, InOutState.WallNormal, InOutState.GravityUp, Hit.ImpactPoint, Dir);
	if (Feature.Kind != EGSTN_WallEdgeKind::Bump)
	{
		return false;
	}
	const float BumpHeight = UGSTN_WallEdgeUtils::BumpHeightOf(Edges, Feature.Offset);
	if (BumpHeight <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Lift = UGSTN_WallEdgeUtils::StepLiftFor(Edges, BumpHeight, CheckSettings.WallGap);
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	const FQuat Quat = UpdatedComponent->GetComponentQuat();

	auto Revert = [&]()
	{
		FHitResult BackHit(1.f);
		MoveRecord.LockRelevancy(false);
		UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, StartLocation - UpdatedComponent->GetComponentLocation(), Quat, false, BackHit, ETeleportType::None, MoveRecord);
		MoveRecord.UnlockRelevancy();
	};

	// Lift off the wall: a pose correction, not travel
	FHitResult LiftHit(1.f);
	MoveRecord.LockRelevancy(false);
	{
		GSTN_WALL_QUERY_SCOPE();
		UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, InOutState.WallNormal * Lift, Quat, true, LiftHit, ETeleportType::None, MoveRecord);
	}
	MoveRecord.UnlockRelevancy();
	if (FVector::DotProduct(UpdatedComponent->GetComponentLocation() - StartLocation, InOutState.WallNormal) < Lift - 0.5f)
	{
		Revert();
		return false;
	}

	// The rest of the move the bump interrupted
	const FVector Remaining = MoveDelta * (1.f - Hit.Time);
	if (!Remaining.IsNearlyZero())
	{
		FHitResult RedoHit(1.f);
		{
			GSTN_WALL_QUERY_SCOPE();
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, Remaining, Quat, true, RedoHit, ETeleportType::None, MoveRecord);
		}
		if (RedoHit.IsValidBlockingHit() && RedoHit.Time < 0.5f && Remaining.Size() > 1.f)
		{
			// Still blocked: it was not what the probe took it for
			Revert();
			return false;
		}
	}

	GSTN_WALL_LOG(1, TEXT("v2 STEP OVER bump=%.1f lift=%.1f"), BumpHeight, Lift);
	InOutState.Orientation = UpdatedComponent->GetComponentQuat();
	return true;
}

// ------------------------------------------------------------------------------------------------
// SimulationTick
// ------------------------------------------------------------------------------------------------

void UGSTN_WallTraverseModeBase::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	UMoverComponent* MoverComp = GetMoverComponent();
	const FMovingComponentSet& MovingComps = Params.MovingComps;
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	if (!MoverComp || !UpdatedComponent || !MovingComps.UpdatedPrimitive.IsValid())
	{
		return;
	}

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	if (UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable())
	{
		// On a wall = no floor (idempotent invalidations, as Mover's own modes)
		SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	const FQuat StartQuat = UpdatedComponent->GetComponentQuat();
	FGSTN_WallTickState TickState = MakeTickState(Params, StartQuat);
	const FVector StartWallNormal = TickState.WallNormal;

	// 1. Ride the dynamic wall, then the voluntary exits (wall jump, detach)
	if (bUseMovementBases)
	{
		ApplyMovementBase(Params, TickState, MoveRecord);
	}
	CheckForWallExit(Params, TickState);
	const bool bLeftOnPurpose = TickState.IsLeaving();

	// 2-5. The final proposed velocity (layered and queued moves included) moves the character
	if (!bLeftOnPurpose)
	{
		TraverseStep(MovingComps, DeltaSeconds, Params.ProposedMove.bHasDirIntent, TickState, MoveRecord);
	}

	// 6. Ridden base, sync state
	UpdateMovementBase(Params, TickState);
	CaptureFinalState(Params, StartQuat, TickState, bLeftOnPurpose, OutputState);

#if GSTN_WALL_DEBUG
	GSTNWallDebug::FinishTick(RegisteredModeName, (Params.TimeStep.BaseSimTimeMs + Params.TimeStep.StepMs) * 0.001, TickState.Velocity.Size(),
		UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(StartWallNormal.GetSafeNormal(), TickState.WallNormal.GetSafeNormal()));
#endif
#if ENABLE_DRAW_DEBUG
	DrawWallDebug(MovingComps, TickState);
#endif
}

void UGSTN_WallTraverseModeBase::TraverseStep(const FMovingComponentSet& MovingComps, float DeltaSeconds, bool bHasDirIntent, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const
{
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	if (!UpdatedComponent || !UpdatedPrimitive || DeltaSeconds <= 0.f)
	{
		return;
	}

	float CapsuleRadius, CapsuleHalfHeight;
	UGSTN_WallQueryUtils::GetCapsuleSize(UpdatedPrimitive, CapsuleRadius, CapsuleHalfHeight);
	const FGSTN_WallCheckSettings CheckSettings = GetWallCheckSettings();

	// Edge hooks: what is ahead, and the speed it allows. The cap clamps the proposed velocity here, the one place geometry may touch speed
	FGSTN_WallAheadResult Ahead = ProbeAhead(MovingComps, InOutState, CheckSettings);
	Ahead.Kind = ClassifyAhead(Ahead);
	const float SpeedCap = ComputeEdgeSpeedCap(Ahead);
	const float Speed = InOutState.Velocity.Size();
	if (Speed > SpeedCap && Speed > UE_KINDA_SMALL_NUMBER)
	{
		InOutState.Velocity *= SpeedCap / Speed;
	}
	GSTN_WALL_LOG(2, TEXT("v2 ahead kind=%d angle=%.1f offset=%.1f dist=%.1f cap=%.0f speed=%.0f"), static_cast<int32>(Ahead.Kind), Ahead.Angle, Ahead.Offset, Ahead.Distance,
		FMath::Min(SpeedCap, 99999.f), Speed);

	// The move: one sweep, at most MaxMoveRadii capsule radii. The facing yaw rides in the same move; it cannot change what
	// the capsule collides with (rotation about its own axis)
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	const FVector MoveDelta = (InOutState.Velocity * DeltaSeconds).GetClampedToMaxSize(CapsuleRadius * MaxMoveRadii);
	const FQuat TargetFacing = ComputeFacingQuat(InOutState.Orientation, InOutState.WallNormal, InOutState.GravityUp);
	const FQuat Facing = UGSTN_WallMovementUtils::SmoothOrientation(InOutState.Orientation, TargetFacing, RotationSharpness, RotationSmoothingDistance,
		MaxRotationRate, MoveDelta.Size(), DeltaSeconds);

	FHitResult MoveHit(1.f);
	if (!MoveDelta.IsNearlyZero() || !InOutState.Orientation.Equals(Facing, 1e-5f))
	{
		if (!MoveDelta.IsNearlyZero())
		{
			GSTN_WALL_QUERY_SCOPE();
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, MoveDelta, Facing, true, MoveHit, ETeleportType::None, MoveRecord);
		}
		else
		{
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, FVector::ZeroVector, Facing, false, MoveHit, ETeleportType::None, MoveRecord);
		}
	}
	InOutState.Orientation = UpdatedComponent->GetComponentQuat();

	FGSTN_WallCornerHit CornerHit;
	if (MoveHit.IsValidBlockingHit() && !HandleMoveBlocked(MovingComps, MoveHit, MoveDelta, DeltaSeconds, Ahead, CheckSettings, InOutState, CornerHit, MoveRecord))
	{
		return;
	}

	// Contact + adhesion in one capsule sweep along the proxy plane's normal
	const FVector PrevWallNormal = InOutState.WallNormal;
	const FVector MovedDelta = UpdatedComponent->GetComponentLocation() - StartLocation;
	FHitResult Contact;
	const bool bContact = UGSTN_WallMovementUtils::AdhereAndSense(MovingComps, InOutState.Orientation, InOutState.WallNormal, InOutState.GravityUp,
		CheckSettings, MaxAdhesionSpeed, DeltaSeconds, MoveRecord, Contact);
	// Overlapping something (resolved by the sweep): neither a wall found nor a wall lost this tick
	const bool bResolvedPenetration = !bContact && Contact.bStartPenetrating;

	FGSTN_WallProbeResult Probe;
	bool bSensed = bContact || CornerHit.IsValid();
	if (bContact)
	{
		// The proxy plane turns only for a real corner or a gentle change. A rounded edge normal of a bump or a facet would otherwise
		// turn the plane toward it for a few ticks, which is exactly the snag on complex geometry
		const bool bRealCorner = Ahead.Kind == EGSTN_WallEdgeKind::Concave || Ahead.Kind == EGSTN_WallEdgeKind::Convex || CornerHit.IsValid();
		// "Gentle" is read from the geometry's own face normal, not the sweep's: touching the rounded corner of a bump with the curved capsule
		// gives a normal that leans 10-20 degrees off the wall, and following it a tick at a time ratchets the plane around the bump
		const FVector FaceNormal = Contact.ImpactNormal.IsNearlyZero() ? FVector(Contact.Normal) : FVector(Contact.ImpactNormal);
		const bool bGentle = UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(FaceNormal, InOutState.WallNormal) <= Edges.MaxSmoothAngle;
		Probe.Normal = bRealCorner ? FVector(Contact.Normal) : (bGentle ? FaceNormal : InOutState.WallNormal);
		Probe.ContactPoint = Contact.ImpactPoint;
		Probe.Component = Contact.GetComponent();
		Probe.BoneName = Contact.BoneName;
		Probe.bValid = true;
	}
	if (CornerHit.IsValid())
	{
		// An inside corner met by the move is the strongest evidence of where the wall goes: the plane turns toward it (rate limited)
		Probe.Normal = CornerHit.Normal;
		Probe.ContactPoint = CornerHit.Point;
		Probe.bValid = true;
	}

	bool bWrapped = false;
	if (!bSensed && !bResolvedPenetration && !MovedDelta.IsNearlyZero())
	{
		// Wall lost: the face around the convex edge we just passed (two sweeps at most, only in this case)
		GSTN_WALL_QUERY_SCOPE();
		GSTN_WALL_QUERY_SCOPE();
		bWrapped = bSensed = UGSTN_WallQueryUtils::FindWallAroundConvexEdge(MovingComps, InOutState.WallNormal, MovedDelta, InOutState.GravityUp, CheckSettings, Probe);
	}

	if (bSensed)
	{
		const bool bAtRest = !bHasDirIntent && InOutState.Velocity.SizeSquared() < FMath::Square(1.f);
		AttachToWall(Probe, bAtRest, DeltaSeconds, InOutState);
		if (bWrapped)
		{
			// The plane now follows the new face: seat the capsule on it
			FHitResult WrapContact;
			UGSTN_WallMovementUtils::AdhereAndSense(MovingComps, InOutState.Orientation, InOutState.WallNormal, InOutState.GravityUp,
				CheckSettings, MaxAdhesionSpeed, DeltaSeconds, MoveRecord, WrapContact);
		}
		InOutState.Orientation = UpdatedComponent->GetComponentQuat();
		// Length preserving: the cap of the edge hook is what removes speed at a corner, never the transport
		InOutState.Velocity = UGSTN_WallMovementUtils::TransportVelocity(InOutState.Velocity, PrevWallNormal, InOutState.WallNormal);
	}
	else if (!bResolvedPenetration)
	{
		InOutState.bHasWall = false;
		InOutState.TimeWithoutWall += DeltaSeconds;
		GSTN_WALL_LOG(1, TEXT("v2 NO WALL loc=%s vel=%s wallN=%s t=%.3f"), *GSTNWallDebug::Vec(UpdatedComponent->GetComponentLocation()),
			*GSTNWallDebug::Vec(InOutState.Velocity), *GSTNWallDebug::Vec(InOutState.WallNormal), InOutState.TimeWithoutWall);
		if (InOutState.TimeWithoutWall >= NoWallGraceTime && !NoWallModeName.IsNone())
		{
			InOutState.LeaveTo(NoWallModeName, 0.f);
		}
	}

	GSTN_WALL_LOG(1, TEXT("v2 loc=%s vel=%s contactN=%s wallN=%s corner=%d wrapped=%d"), *GSTNWallDebug::Vec(UpdatedComponent->GetComponentLocation()),
		*GSTNWallDebug::Vec(InOutState.Velocity), *GSTNWallDebug::Vec(Probe.Normal), *GSTNWallDebug::Vec(InOutState.WallNormal), CornerHit.IsValid() ? 1 : 0, bWrapped ? 1 : 0);
}

bool UGSTN_WallTraverseModeBase::HandleMoveBlocked(const FMovingComponentSet& MovingComps, const FHitResult& BlockingHit, const FVector& MoveDelta, float DeltaSeconds,
	const FGSTN_WallAheadResult& Ahead, const FGSTN_WallCheckSettings& CheckSettings, FGSTN_WallTickState& InOutState,
	FGSTN_WallCornerHit& OutCornerHit, FMovementRecord& MoveRecord) const
{
	FHitResult Hit = BlockingHit;
	if (UMoverComponent* MoverComp = GetMoverComponent())
	{
		FMoverOnImpactParams ImpactParams(RegisteredModeName, Hit, MoveDelta);
		MoverComp->HandleImpact(ImpactParams);
	}

	// A low feature is not an obstacle: step over it before any corner or floor logic can read it as one (a small ledge below must not
	// end the slide, a real floor must)
	if (TryStepOverBump(MovingComps, Hit, MoveDelta, Ahead, DeltaSeconds, InOutState, MoveRecord))
	{
		return true;
	}

	// Moving into another climbable surface = inside corner (the plane turns toward it); down into a flat surface = landing
	bool bIsConcaveCorner;
	{
		GSTN_WALL_QUERY_SCOPE();
		bIsConcaveCorner = UGSTN_WallQueryUtils::IsConcaveCornerHit(MovingComps, Hit, MoveDelta, InOutState.Orientation, InOutState.WallNormal,
			InOutState.GravityUp, NormalDeadzoneAngle, CheckSettings);
	}
	const bool bIsFloor = !bIsConcaveCorner && UGSTN_WallQueryUtils::IsFloorHit(Hit, MoveDelta, InOutState.GravityUp, MinSurfaceAngle);
	if (bIsFloor && !FloorModeName.IsNone())
	{
		InOutState.LeaveTo(FloorModeName, (1.f - Hit.Time) * DeltaSeconds);
		InOutState.Velocity = FVector::VectorPlaneProject(InOutState.Velocity, InOutState.GravityUp);
		return false;
	}

	if (bIsConcaveCorner)
	{
		OutCornerHit.Normal = Hit.Normal;
		OutCornerHit.Point = Hit.ImpactPoint;
	}
	else if (FVector::DotProduct(InOutState.Velocity, Hit.Normal) < 0.f)
	{
		// Not climbable (too steep, a prop...): stop pushing into it, no friction term of its own
		InOutState.Velocity = FVector::VectorPlaneProject(InOutState.Velocity, Hit.Normal);
	}

	{
		GSTN_WALL_QUERY_SCOPE();
		UMovementUtils::TryMoveToSlideAlongSurface(MovingComps, MoveDelta, 1.f - Hit.Time, InOutState.Orientation, Hit.Normal, Hit, true, MoveRecord);
	}
	InOutState.Orientation = MovingComps.UpdatedComponent->GetComponentQuat();
	return true;
}
