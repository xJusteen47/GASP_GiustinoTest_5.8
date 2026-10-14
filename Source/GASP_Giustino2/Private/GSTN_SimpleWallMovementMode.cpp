// Fill out your copyright notice in the Description page of Project Settings.


#include "GSTN_SimpleWallMovementMode.h"

#include "DrawDebugHelpers.h"
#include "GSTN_WallMovementDebug.h"
#include "Engine/NetSerialization.h"
#include "Engine/World.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SimpleWallMovementMode)

namespace GSTNWall
{
	// Shrink applied to the capsule when validating a rotated pose, so resting contact with the wall doesn't count as overlap
	constexpr float OverlapTestShrink = 0.5f;

	/** Rotates A toward B around Axis (both assumed perpendicular to Axis) by Alpha of their signed angle */
	static FVector RotateTowardsInPlane(const FVector& A, const FVector& B, const FVector& Axis, float Alpha)
	{
		const float Angle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(A, B), Axis), FVector::DotProduct(A, B));
		return FQuat(Axis, Angle * Alpha).RotateVector(A);
	}

	/** Projects V onto the plane, keeping its original length */
	static FVector ProjectOnPlaneKeepLength(const FVector& V, const FVector& PlaneNormal)
	{
		const float Length = V.Size();
		const FVector Projected = FVector::VectorPlaneProject(V, PlaneNormal);
		return Projected.GetSafeNormal() * Length;
	}

	/** Alpha for exponential smoothing that is independent of the step size */
	static float ExpAlpha(float Sharpness, float Step)
	{
		return 1.f - FMath::Exp(-Sharpness * Step);
	}

	/**
	 * Whether the capsule fits on a new wall met at an inside corner: turned to face it and placed at WallGap from it,
	 * it must not overlap anything. Rejects faces narrower than the capsule (small steps, slanted niches), which would
	 * otherwise be accepted as walls and leave the capsule wedged half-rotated between the two surfaces.
	 */
	static bool CanFitOnWall(const FMovingComponentSet& MovingComps, const FQuat& CurrentQuat, const FVector& NewNormal,
		const FVector& ContactPoint, float Radius, float HalfHeight, float WallGap)
	{
		const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
		const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
		if (!UpdatedComponent || !UpdatedPrimitive)
		{
			return false;
		}

		FVector Up = FVector::VectorPlaneProject(CurrentQuat.GetUpVector(), NewNormal).GetSafeNormal();
		if (Up.IsNearlyZero())
		{
			Up = FVector::VectorPlaneProject(CurrentQuat.GetForwardVector(), NewNormal).GetSafeNormal();
		}
		const FQuat TestQuat = FRotationMatrix::MakeFromXZ(-NewNormal, Up).ToQuat();

		FVector TestLocation = UpdatedComponent->GetComponentLocation();
		TestLocation += NewNormal * ((Radius + WallGap) - FVector::DotProduct(TestLocation - ContactPoint, NewNormal));

		constexpr float FitTestShrink = 1.f;
		const FCollisionShape TestShape = FCollisionShape::MakeCapsule(FMath::Max(Radius - FitTestShrink, 1.f), FMath::Max(HalfHeight - FitTestShrink, 1.f));
		return !UMovementUtils::OverlapTest(UpdatedComponent, UpdatedPrimitive, TestLocation, TestQuat,
			UpdatedPrimitive->GetCollisionObjectType(), TestShape, UpdatedComponent->GetOwner());
	}
}

// ------------------------------------------------------------------------------------------------
// FGSTN_WallMovementSyncState
// ------------------------------------------------------------------------------------------------

FMoverDataStructBase* FGSTN_WallMovementSyncState::Clone() const
{
	return new FGSTN_WallMovementSyncState(*this);
}

bool FGSTN_WallMovementSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	uint8 bHasWallBit = bHasWall ? 1 : 0;
	Ar.SerializeBits(&bHasWallBit, 1);
	bHasWall = (bHasWallBit != 0);

	SerializeFixedVector<1, 16>(WallNormal, Ar);
	SerializePackedVector<100, 30>(WallContactPoint, Ar);
	Ar << TimeWithoutWall;

	bOutSuccess = true;
	return true;
}

void FGSTN_WallMovementSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("HasWall: %d  TimeWithoutWall: %.2f\n", bHasWall ? 1 : 0, TimeWithoutWall);
	Out.Appendf("WallNormal: X=%.3f Y=%.3f Z=%.3f\n", WallNormal.X, WallNormal.Y, WallNormal.Z);
	Out.Appendf("WallContact: X=%.2f Y=%.2f Z=%.2f\n", WallContactPoint.X, WallContactPoint.Y, WallContactPoint.Z);
}

bool FGSTN_WallMovementSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FGSTN_WallMovementSyncState& Authority = static_cast<const FGSTN_WallMovementSyncState&>(AuthorityState);
	constexpr float NormalCosTolerance = 0.996f; // ~5 degrees

	return bHasWall != Authority.bHasWall
		|| FVector::DotProduct(WallNormal, Authority.WallNormal) < NormalCosTolerance;
}

void FGSTN_WallMovementSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FGSTN_WallMovementSyncState& FromState = static_cast<const FGSTN_WallMovementSyncState&>(From);
	const FGSTN_WallMovementSyncState& ToState = static_cast<const FGSTN_WallMovementSyncState&>(To);

	const FVector LerpedNormal = FMath::Lerp(FromState.WallNormal, ToState.WallNormal, Pct);
	WallNormal = LerpedNormal.GetSafeNormal(UE_SMALL_NUMBER, ToState.WallNormal);
	WallContactPoint = FMath::Lerp(FromState.WallContactPoint, ToState.WallContactPoint, Pct);
	TimeWithoutWall = FMath::Lerp(FromState.TimeWithoutWall, ToState.TimeWithoutWall, Pct);
	bHasWall = ToState.bHasWall;
}

// ------------------------------------------------------------------------------------------------
// UGSTN_SimpleWallMovementMode
// ------------------------------------------------------------------------------------------------

UGSTN_SimpleWallMovementMode::UGSTN_SimpleWallMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = false;
	NoWallModeName = DefaultModeNames::Falling;
	FloorModeName = DefaultModeNames::Walking;
}

void UGSTN_SimpleWallMovementMode::OnRegistered(const FName ModeName, const FMoverSimContext& SimContext)
{
	Super::OnRegistered(ModeName, SimContext);
	RegisteredModeName = ModeName;
}

void UGSTN_SimpleWallMovementMode::Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	if (!OutSyncState)
	{
		return;
	}

	// Reset wall state: until the first probe succeeds we assume the wall is in front of the capsule.
	// Entry transitions (Climbing etc.) are expected to orient the character toward the wall before activating.
	FVector Forward = FVector::ForwardVector;
	if (const FMoverDefaultSyncState* DefaultSync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		Forward = DefaultSync->GetOrientation_WorldSpace().Vector();
	}

	FGSTN_WallMovementSyncState& WallState = OutSyncState->SyncStateCollection.FindOrAddMutableDataByType<FGSTN_WallMovementSyncState>();
	WallState.WallNormal = -Forward;
	WallState.WallContactPoint = FVector::ZeroVector;
	WallState.bHasWall = false;
	WallState.TimeWithoutWall = 0.f;
}

// ------------------------------------------------------------------------------------------------
// GenerateMove: intent -> velocity in the wall plane
// ------------------------------------------------------------------------------------------------

void UGSTN_SimpleWallMovementMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FGSTN_WallMovementSyncState* WallState = StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector GravityUp = MoverComp->GetUpDirection();
	const FQuat Orientation = StartingSyncState->GetOrientation_WorldSpace().Quaternion();

	// The movement plane is the filtered wall normal, not the (lagging, smoothed) capsule orientation
	const FVector WallNormal = (WallState && !WallState->WallNormal.IsNearlyZero()) ? WallState->WallNormal : -Orientation.GetForwardVector();

	const FVector WallIntent = MapInputToWall(CharacterInputs, Orientation, WallNormal, GravityUp);
	const FVector TargetVelocity = ComputeTargetVelocity(WallIntent, Orientation, WallNormal, GravityUp);

	// Prior velocity was already transported onto the wall at the end of last tick; project again in case the state came from elsewhere.
	// Plain projection (not length-preserving): a velocity coming from another mode, mostly into the wall, must not turn into full speed along it
	const FVector PriorVelocity = FVector::VectorPlaneProject(StartingSyncState->GetVelocity_WorldSpace(), WallNormal);

	OutProposedMove.LinearVelocity = ComputeWallVelocity(PriorVelocity, TargetVelocity, DeltaSeconds);

	GSTN_WALL_LOG(1, TEXT("GEN startVel=%s prior=%s intent=%s target=%s out=%s ctrlYaw=%.1f"),
		*GSTNWallDebug::Vec(StartingSyncState->GetVelocity_WorldSpace()), *GSTNWallDebug::Vec(PriorVelocity), *GSTNWallDebug::Vec(WallIntent), *GSTNWallDebug::Vec(TargetVelocity),
		*GSTNWallDebug::Vec(OutProposedMove.LinearVelocity), CharacterInputs ? CharacterInputs->ControlRotation.Yaw : 0.f);
	OutProposedMove.DirectionIntent = WallIntent;
	OutProposedMove.bHasDirIntent = !WallIntent.IsNearlyZero();
	// Orientation on a wall is dictated by geometry, not by input: it's resolved in SimulationTick
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
}

FVector UGSTN_SimpleWallMovementMode::MapInputToWall(const FCharacterDefaultInputs* Inputs, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp) const
{
#if GSTN_WALL_DEBUG
	// gstn.Wall.DebugInput: drive the mode without a keyboard (see GSTN_WallMovementDebug.h)
	FVector DebugValue;
	const GSTNWallDebug::EInputOverride DebugOverride = GSTNWallDebug::GetInputOverride(DebugValue);
	if (DebugOverride == GSTNWallDebug::EInputOverride::World)
	{
		return FVector::VectorPlaneProject(DebugValue, WallNormal).GetClampedToMaxSize(1.f);
	}
#endif

	if (!Inputs)
	{
		return FVector::ZeroVector;
	}

	FVector Input = UMovementUtils::ComputeDirectionIntent(Inputs->GetMoveInput_WorldSpace(), Inputs->GetMoveInputType(), MaxSpeed);

#if GSTN_WALL_DEBUG
	if (DebugOverride == GSTNWallDebug::EInputOverride::Stick)
	{
		const FVector CamForward = FVector::VectorPlaneProject(Inputs->ControlRotation.Vector(), GravityUp).GetSafeNormal();
		const FVector CamRight = FVector::CrossProduct(GravityUp, CamForward);
		Input = CamRight * DebugValue.X + CamForward * DebugValue.Y;
	}
#endif

	Input = Input.GetClampedToMaxSize(1.f);
	if (Input.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector TangentUp, TangentRight;
	GetWallTangentBasis(Orientation, WallNormal, TangentUp, TangentRight);

	// Fallback for degenerate cases: plain projection onto the wall plane
	const FVector ProjectedInput = FVector::VectorPlaneProject(Input, WallNormal).GetClampedToMaxSize(1.f);

	// Recover the stick input (camera space) from the world input. The world input is the stick rotated by the camera yaw:
	// interpreting it in world space would make the same key mean a different wall direction as the capsule turns around corners
	const FQuat ControlQuat = Inputs->ControlRotation.Quaternion();
	const FVector CamForwardFlat = FVector::VectorPlaneProject(ControlQuat.GetForwardVector(), GravityUp).GetSafeNormal();
	if (CamForwardFlat.IsNearlyZero())
	{
		return ProjectedInput;
	}
	const FVector CamRightFlat = FVector::CrossProduct(GravityUp, CamForwardFlat);
	const float StickRight = FVector::DotProduct(Input, CamRightFlat);
	const float StickForward = FVector::DotProduct(Input, CamForwardFlat);

	if (InputMapping == EGSTN_WallInputMapping::CameraRelative)
	{
		// "Screen up" on the wall: Forward+Up of the camera projected on the plane. Walls in front -> camera up, ceilings -> camera forward
		FVector ScreenUp = FVector::VectorPlaneProject(ControlQuat.GetForwardVector() + ControlQuat.GetUpVector(), WallNormal).GetSafeNormal();
		if (ScreenUp.IsNearlyZero())
		{
			ScreenUp = TangentUp;
		}
		const FVector ScreenRight = FVector::CrossProduct(WallNormal, ScreenUp);

		return (ScreenRight * StickRight + ScreenUp * StickForward).GetClampedToMaxSize(1.f);
	}

	// SurfaceRelative: the stick drives the capsule's own frame on the wall (right = sideways, forward = up the wall).
	// Independent of where the capsule faces in the world, so the same key keeps the same meaning around every corner
	return (TangentRight * StickRight + TangentUp * StickForward).GetClampedToMaxSize(1.f);
}

FVector UGSTN_SimpleWallMovementMode::ComputeTargetVelocity(const FVector& WallIntent, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp) const
{
	FVector TangentUp, TangentRight;
	GetWallTangentBasis(Orientation, WallNormal, TangentUp, TangentRight);

	const float UpAmount = FVector::DotProduct(WallIntent, TangentUp);
	const float RightAmount = FVector::DotProduct(WallIntent, TangentRight);
	const float VerticalScale = (UpAmount >= 0.f) ? UpSpeedScale : DownSpeedScale;

	return (TangentUp * UpAmount * VerticalScale + TangentRight * RightAmount * LateralSpeedScale) * MaxSpeed;
}

FVector UGSTN_SimpleWallMovementMode::ComputeWallVelocity(const FVector& PriorVelocity, const FVector& TargetVelocity, float DeltaSeconds) const
{
	if (DeltaSeconds <= 0.f)
	{
		return PriorVelocity;
	}

	// Constant-rate "move towards" in the plane: no overshoot, smooth curves when changing direction
	float Rate = Acceleration;
	if (TargetVelocity.IsNearlyZero()
		|| FVector::DotProduct(PriorVelocity, TargetVelocity) < 0.f
		|| PriorVelocity.SizeSquared() > TargetVelocity.SizeSquared())
	{
		// No input, reversing, or over the target speed -> brake with deceleration
		Rate = FMath::Max(Acceleration, Deceleration);
		if (TargetVelocity.IsNearlyZero())
		{
			Rate = Deceleration;
		}
	}

	const FVector Diff = TargetVelocity - PriorVelocity;
	const float MaxStep = Rate * DeltaSeconds;
	const float DiffSize = Diff.Size();
	if (DiffSize <= MaxStep || DiffSize <= UE_KINDA_SMALL_NUMBER)
	{
		return TargetVelocity;
	}
	return PriorVelocity + Diff * (MaxStep / DiffSize);
}

bool UGSTN_SimpleWallMovementMode::IsValidWallNormal(const FVector& Normal, const FVector& GravityUp) const
{
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal, GravityUp), -1.f, 1.f)));
	return AngleDeg >= MinSurfaceAngle && AngleDeg <= MaxSurfaceAngle;
}

bool UGSTN_SimpleWallMovementMode::IsHitOnValidWall(const UWorld* World, const FHitResult& Hit, const FVector& GravityUp, ECollisionChannel Channel,
	const FCollisionQueryParams& QueryParams, const FCollisionResponseParams& ResponseParams) const
{
	if (!IsValidWallNormal(Hit.Normal, GravityUp) || !IsValidWallNormal(Hit.ImpactNormal, GravityUp))
	{
		return false;
	}

	// Face contact: the sweep normal is the face normal, nothing else to check
	constexpr float EdgeCosThreshold = 0.9994f; // ~2 degrees
	if (FVector::DotProduct(Hit.Normal, Hit.ImpactNormal) >= EdgeCosThreshold || !World)
	{
		return true;
	}

	// Edge contact: ImpactNormal is only one of the two faces. Step just past the edge, into the reported face, which lands
	// on the plane of the other face, and line trace it from outside along the rounded normal to read its real normal
	constexpr float StepPastEdge = 2.f;
	constexpr float TraceHalfLength = 10.f;
	const FVector Target = Hit.ImpactPoint - Hit.ImpactNormal * StepPastEdge;

	FHitResult FaceHit;
	if (World->LineTraceSingleByChannel(FaceHit, Target + Hit.Normal * TraceHalfLength, Target - Hit.Normal * TraceHalfLength,
		Channel, QueryParams, ResponseParams) && !FaceHit.bStartPenetrating)
	{
		return IsValidWallNormal(FaceHit.ImpactNormal, GravityUp);
	}

	// Other face not found (e.g. a vertex): trust the reported face, which is a valid wall
	return true;
}

// ------------------------------------------------------------------------------------------------
// SimulationTick: move, sense, orient, adhere
// ------------------------------------------------------------------------------------------------

void UGSTN_SimpleWallMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	UMoverComponent* MoverComp = GetMoverComponent();
	const FMovingComponentSet& MovingComps = Params.MovingComps;
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();

	if (!MoverComp || !UpdatedComponent || !UpdatedPrimitive)
	{
		return;
	}

	const FMoverTickStartData& StartState = Params.StartState;
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);
	const FGSTN_WallMovementSyncState* StartWallState = StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	FGSTN_WallMovementSyncState& OutputWallState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_WallMovementSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	if (UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable())
	{
		// On a wall = no floor
		SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	const FVector GravityUp = MoverComp->GetUpDirection();
	const FQuat StartQuat = UpdatedComponent->GetComponentQuat();
	FQuat CurrentQuat = StartQuat;

	FVector WallNormal = (StartWallState && !StartWallState->WallNormal.IsNearlyZero()) ? StartWallState->WallNormal : -StartQuat.GetForwardVector();
	FVector WallContactPoint = StartWallState ? StartWallState->WallContactPoint : FVector::ZeroVector;
	bool bHasWall = StartWallState ? StartWallState->bHasWall : false;
	float TimeWithoutWall = StartWallState ? StartWallState->TimeWithoutWall : 0.f;

	FVector Velocity = Params.ProposedMove.LinearVelocity;
	OutputSyncState.MoveDirectionIntent = Params.ProposedMove.bHasDirIntent ? Params.ProposedMove.DirectionIntent : FVector::ZeroVector;

	float CapsuleRadius, CapsuleHalfHeight;
	GetCapsuleSize(UpdatedPrimitive, CapsuleRadius, CapsuleHalfHeight);

	// Substepping: corners must be sensed every few centimetres or fast movement skips past them
	const float SubstepLength = (MaxSubstepDistance > 0.f) ? MaxSubstepDistance : FMath::Max(CapsuleRadius * 0.5f, 1.f);
	const float TotalDistance = Velocity.Size() * DeltaSeconds;
	const int32 NumSubsteps = FMath::Clamp(FMath::CeilToInt(TotalDistance / SubstepLength), 1, MaxSubsteps);
	const float SubDeltaSeconds = DeltaSeconds / NumSubsteps;

	const UWorld* World = UpdatedComponent->GetWorld();
	const ECollisionChannel CollisionChannel = UpdatedPrimitive->GetCollisionObjectType();
	FCollisionQueryParams EdgeQueryParams(SCENE_QUERY_STAT(GSTN_WallEdgeCheck), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams EdgeResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, EdgeQueryParams, EdgeResponseParams);

	// Mode switch decided during the substeps, with the simulation time it leaves unused
	FName NextModeName = NAME_None;
	float UnusedSeconds = 0.f;

	for (int32 SubstepIdx = 0; SubstepIdx < NumSubsteps && DeltaSeconds > 0.f; ++SubstepIdx)
	{
		// 1. Translate along the wall tangent ------------------------------------------------
		const FVector SubDelta = Velocity * SubDeltaSeconds;
		FVector ConcaveHitNormal = FVector::ZeroVector;
		FVector ConcaveHitPoint = FVector::ZeroVector;

		if (!SubDelta.IsNearlyZero())
		{
			FHitResult Hit(1.f);
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, SubDelta, CurrentQuat, true, Hit, ETeleportType::None, MoveRecord);

			if (Hit.IsValidBlockingHit())
			{
				FMoverOnImpactParams ImpactParams(RegisteredModeName, Hit, SubDelta);
				MoverComp->HandleImpact(ImpactParams);

				// Moving into another climbable surface = inside corner. Feed it to the probe as a strong candidate
				const float CornerAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Hit.Normal, WallNormal), -1.f, 1.f)));
				const bool bIsConcaveCorner = FVector::DotProduct(SubDelta, Hit.Normal) < 0.f
					&& CornerAngle > NormalDeadzoneAngle
					&& CornerAngle <= MaxConcaveAngle
					&& IsHitOnValidWall(World, Hit, GravityUp, CollisionChannel, EdgeQueryParams, EdgeResponseParams)
					&& GSTNWall::CanFitOnWall(MovingComps, CurrentQuat, Hit.Normal, Hit.ImpactPoint, CapsuleRadius, CapsuleHalfHeight, WallGap);

				// Moving down into a surface flatter than a wall = landing on a floor
				const bool bIsFloor = !bIsConcaveCorner
					&& FVector::DotProduct(Hit.ImpactNormal, GravityUp) >= FMath::Cos(FMath::DegreesToRadians(MinSurfaceAngle))
					&& FVector::DotProduct(SubDelta, GravityUp) < 0.f;

				if (bIsFloor && !FloorModeName.IsNone())
				{
					NextModeName = FloorModeName;
					UnusedSeconds = (1.f - Hit.Time) * SubDeltaSeconds + (NumSubsteps - SubstepIdx - 1) * SubDeltaSeconds;
					Velocity = FVector::VectorPlaneProject(Velocity, GravityUp);
					break;
				}

				if (bIsConcaveCorner)
				{
					ConcaveHitNormal = Hit.Normal;
					ConcaveHitPoint = Hit.ImpactPoint;
				}
				else if (FVector::DotProduct(Velocity, Hit.Normal) < 0.f)
				{
					// Non-climbable obstacle (floor, too steep, etc.): stop pushing into it
					Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
				}

				UMovementUtils::TryMoveToSlideAlongSurface(MovingComps, SubDelta, 1.f - Hit.Time, CurrentQuat, Hit.Normal, Hit, true, MoveRecord);
			}
		}

		// 2-3. Sense the wall -------------------------------------------------------------------
		FGSTN_WallProbeResult Probe;
		bool bFoundWall = ProbeWall(MovingComps, CurrentQuat, WallNormal, Velocity, ConcaveHitNormal, ConcaveHitPoint, GravityUp, SubDeltaSeconds, Probe);
		if (!bFoundWall && !SubDelta.IsNearlyZero())
		{
			bFoundWall = ProbeConvexWrap(MovingComps, WallNormal, SubDelta, GravityUp, Probe);
		}

		if (!bFoundWall)
		{
			bHasWall = false;
			TimeWithoutWall += SubDeltaSeconds;
			GSTN_WALL_LOG(1, TEXT("[%d/%d] NO WALL loc=%s vel=%s wallN=%s t=%.3f"),
				SubstepIdx, NumSubsteps, *GSTNWallDebug::Vec(UpdatedComponent->GetComponentLocation()), *GSTNWallDebug::Vec(Velocity), *GSTNWallDebug::Vec(WallNormal), TimeWithoutWall);

			if (TimeWithoutWall >= NoWallGraceTime && !NoWallModeName.IsNone())
			{
				NextModeName = NoWallModeName;
				UnusedSeconds = (NumSubsteps - SubstepIdx - 1) * SubDeltaSeconds;
				break;
			}
			continue;
		}

		// 4. Soft deadzone: tiny normal changes (surface noise) are mostly ignored, big ones pass through untouched.
		//    Then rate-limit: a probe sample appearing or vanishing must not snap the movement plane in a single step.
		//    At rest the normal is frozen once the wall has been acquired: wedged between two surfaces (inside corners, props
		//    against the wall) normal -> orientation/adhesion -> position -> probes is a feedback loop that can oscillate on its
		//    own, and with no movement there is nothing new to sense anyway. Probes keep running to detect losing the wall.
		const FVector PrevWallNormal = WallNormal;
		const bool bAtRest = !Params.ProposedMove.bHasDirIntent && Velocity.SizeSquared() < FMath::Square(1.f);
		const bool bHadWall = bHasWall;
		if (!(bAtRest && bHadWall))
		{
			const float ChangeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Probe.Normal, WallNormal), -1.f, 1.f)));
			float Blend = (NormalDeadzoneAngle > 0.f) ? FMath::Square(FMath::Clamp(ChangeDeg / NormalDeadzoneAngle, 0.f, 1.f)) : 1.f;
			const float MaxStepDeg = MaxNormalChangeRate * SubDeltaSeconds;
			if (MaxNormalChangeRate > 0.f && ChangeDeg * Blend > MaxStepDeg)
			{
				Blend = MaxStepDeg / ChangeDeg;
			}
			const FQuat NormalDelta = FQuat::FindBetweenNormals(WallNormal, Probe.Normal);
			WallNormal = FQuat::Slerp(FQuat::Identity, NormalDelta, Blend).RotateVector(WallNormal).GetSafeNormal();
		}
		WallContactPoint = Probe.ContactPoint;
		bHasWall = true;
		TimeWithoutWall = 0.f;

		// 5-6. Orientation: target from parallel transport + biases, smoothed by time AND distance, applied around the contact point
		{
			const FQuat TargetQuat = ComputeTargetOrientation(CurrentQuat, WallNormal, GravityUp, Velocity, SubDeltaSeconds);

			const float TimeAlpha = GSTNWall::ExpAlpha(RotationSharpness, SubDeltaSeconds);
			const float DistAlpha = GSTNWall::ExpAlpha(1.f / RotationSmoothingDistance, SubDelta.Size());
			FQuat SmoothedQuat = FQuat::Slerp(CurrentQuat, TargetQuat, FMath::Max(TimeAlpha, DistAlpha)).GetNormalized();

			const float StepAngle = CurrentQuat.AngularDistance(SmoothedQuat);
			const float MaxStepAngle = FMath::DegreesToRadians(MaxRotationRate) * SubDeltaSeconds;
			if (StepAngle > MaxStepAngle && StepAngle > UE_KINDA_SMALL_NUMBER)
			{
				SmoothedQuat = FQuat::Slerp(CurrentQuat, SmoothedQuat, MaxStepAngle / StepAngle).GetNormalized();
			}

			if (!CurrentQuat.Equals(SmoothedQuat, 1e-5f))
			{
				CurrentQuat = ApplyRotationAroundPivot(MovingComps, CurrentQuat, SmoothedQuat, WallContactPoint, MoveRecord);
			}
		}

		// 7. Adhesion -----------------------------------------------------------------------
		[[maybe_unused]] const FVector PreAdhereLoc = UpdatedComponent->GetComponentLocation();
		AdhereToWall(MovingComps, CurrentQuat, WallNormal, GravityUp, SubDeltaSeconds, MoveRecord);

		GSTN_WALL_LOG(1, TEXT("[%d/%d] loc=%s rot=%s vel=%s probeN=%s wallN=%s concave=%d adhere=%s up=%s"),
			SubstepIdx, NumSubsteps, *GSTNWallDebug::Vec(UpdatedComponent->GetComponentLocation()), *CurrentQuat.Rotator().ToCompactString(),
			*GSTNWallDebug::Vec(Velocity), *GSTNWallDebug::Vec(Probe.Normal), *GSTNWallDebug::Vec(WallNormal), ConcaveHitNormal.IsNearlyZero() ? 0 : 1,
			*GSTNWallDebug::Vec(UpdatedComponent->GetComponentLocation() - PreAdhereLoc), *GSTNWallDebug::Vec(CurrentQuat.GetUpVector()));

		// 8. Parallel-transport the velocity onto the new plane: same speed, direction carried around the corner
		if (!Velocity.IsNearlyZero())
		{
			const FVector Transported = FQuat::FindBetweenNormals(PrevWallNormal, WallNormal).RotateVector(Velocity);
			Velocity = GSTNWall::ProjectOnPlaneKeepLength(Transported, WallNormal);
		}
	}

	// Final state ------------------------------------------------------------------------------
	const FQuat FinalQuat = UpdatedComponent->GetComponentQuat();
	FVector AngularVelocityDegrees = FVector::ZeroVector;
	if (DeltaSeconds > 0.f)
	{
		FQuat DeltaQuat = FinalQuat * StartQuat.Inverse();
		DeltaQuat.EnforceShortestArcWith(FQuat::Identity);
		FVector Axis;
		float AngleRad;
		DeltaQuat.ToAxisAndAngle(Axis, AngleRad);
		AngularVelocityDegrees = Axis * FMath::RadiansToDegrees(AngleRad) / DeltaSeconds;
	}

	// The intended (transported) velocity is used instead of the positional delta: the latter includes adhesion corrections and would jitter
	OutputSyncState.SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(), FinalQuat.Rotator(), Velocity, AngularVelocityDegrees, nullptr);
	UpdatedComponent->ComponentVelocity = Velocity;

	OutputWallState.WallNormal = WallNormal;
	OutputWallState.WallContactPoint = WallContactPoint;
	OutputWallState.bHasWall = bHasWall;
	OutputWallState.TimeWithoutWall = TimeWithoutWall;

	if (!NextModeName.IsNone())
	{
		OutputState.MovementEndState.NextModeName = NextModeName;
		OutputState.MovementEndState.RemainingMs = FMath::Clamp(UnusedSeconds * 1000.f, 0.f, Params.TimeStep.StepMs);
		GSTN_WALL_LOG(1, TEXT("-> switching to %s (remaining %.1f ms)"),
			*NextModeName.ToString(), OutputState.MovementEndState.RemainingMs);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && IsInGameThread())
	{
		const UWorld* InWorld = UpdatedComponent->GetWorld();
		const FVector Loc = UpdatedComponent->GetComponentLocation();
		DrawDebugCapsule(InWorld, Loc, CapsuleHalfHeight, CapsuleRadius, FinalQuat, bHasWall ? FColor::Green : FColor::Red, false, -1.f, 0, 0.5f);
		DrawDebugDirectionalArrow(InWorld, WallContactPoint, WallContactPoint + WallNormal * 40.f, 8.f, FColor::Cyan, false, -1.f, 0, 1.f);
		DrawDebugDirectionalArrow(InWorld, Loc, Loc + FinalQuat.GetUpVector() * 50.f, 8.f, FColor::Blue, false, -1.f, 0, 1.f);
		DrawDebugDirectionalArrow(InWorld, Loc, Loc + Velocity * 0.25f, 8.f, FColor::Yellow, false, -1.f, 0, 1.f);
	}
#endif
}

// ------------------------------------------------------------------------------------------------
// Orientation
// ------------------------------------------------------------------------------------------------

FQuat UGSTN_SimpleWallMovementMode::ComputeTargetOrientation(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const
{
	// 1. Parallel transport: the minimal rotation that brings the current facing (-Forward) onto the wall (-Normal).
	//    It changes only what the geometry forces and keeps the heading on the wall, so there are no flips at ceilings or corners
	const FQuat Transport = FQuat::FindBetweenNormals(CurrentQuat.GetForwardVector(), -WallNormal);
	FVector Up = FVector::VectorPlaneProject(Transport.RotateVector(CurrentQuat.GetUpVector()), WallNormal).GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		Up = FVector::VectorPlaneProject(GravityUp, WallNormal).GetSafeNormal();
		if (Up.IsNearlyZero())
		{
			Up = FVector::VectorPlaneProject(CurrentQuat.GetRightVector(), WallNormal).GetSafeNormal();
		}
	}

	// 2. Upright bias: on walls, roll the capsule (around the wall normal) toward gravity-up projected on the wall.
	//    Its strength scales with how vertical the wall is, so it fades smoothly to zero on ceilings, where "up" is undefined
	const FVector GravityTangent = FVector::VectorPlaneProject(GravityUp, WallNormal);
	const float Verticality = GravityTangent.Size(); // 1 = vertical wall, 0 = ceiling/floor
	if (Verticality > UE_KINDA_SMALL_NUMBER && UprightSharpness > 0.f)
	{
		const float UprightAlpha = GSTNWall::ExpAlpha(UprightSharpness * FMath::Square(Verticality), DeltaSeconds);
		Up = GSTNWall::RotateTowardsInPlane(Up, GravityTangent / Verticality, WallNormal, UprightAlpha);
	}

	// 3. Ceiling heading: where the upright bias fades, turn toward the movement direction instead (like a walking heading)
	if (bAlignHeadingToMovementOnCeilings && FVector::DotProduct(WallNormal, GravityUp) < 0.f)
	{
		const FVector MoveDir = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal();
		const float SpeedFactor = FMath::Clamp(Velocity.Size() / FMath::Max(MaxSpeed * 0.5f, 1.f), 0.f, 1.f);
		const float CeilingFactor = 1.f - Verticality;
		if (!MoveDir.IsNearlyZero() && SpeedFactor > 0.f)
		{
			const float HeadingAlpha = GSTNWall::ExpAlpha(CeilingHeadingSharpness * CeilingFactor * SpeedFactor, DeltaSeconds);
			Up = GSTNWall::RotateTowardsInPlane(Up, MoveDir, WallNormal, HeadingAlpha);
		}
	}

	// X faces the wall, Z runs along it
	return FRotationMatrix::MakeFromXZ(-WallNormal, Up).ToQuat();
}

FQuat UGSTN_SimpleWallMovementMode::ApplyRotationAroundPivot(const FMovingComponentSet& MovingComps, const FQuat& FromQuat, const FQuat& ToQuat, const FVector& Pivot, FMovementRecord& MoveRecord) const
{
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();

	float Radius, HalfHeight;
	GetCapsuleSize(UpdatedPrimitive, Radius, HalfHeight);
	const FCollisionShape TestShape = FCollisionShape::MakeCapsule(
		FMath::Max(Radius - GSTNWall::OverlapTestShrink, 1.f),
		FMath::Max(HalfHeight - GSTNWall::OverlapTestShrink, 1.f));

	const FVector Location = UpdatedComponent->GetComponentLocation();

	// Rotating around the wall contact (instead of the capsule center) makes the capsule roll over edges
	// rather than swing its ends into the geometry. If the pose still overlaps, back off the rotation.
	static constexpr float Fractions[] = { 1.f, 0.5f, 0.25f };
	for (const float Fraction : Fractions)
	{
		const FQuat CandidateQuat = FQuat::Slerp(FromQuat, ToQuat, Fraction).GetNormalized();
		const FQuat DeltaRotation = CandidateQuat * FromQuat.Inverse();
		const FVector CandidateLocation = Pivot + DeltaRotation.RotateVector(Location - Pivot);

		const bool bBlocked = UMovementUtils::OverlapTest(UpdatedComponent, UpdatedPrimitive, CandidateLocation, CandidateQuat,
			UpdatedPrimitive->GetCollisionObjectType(), TestShape, UpdatedComponent->GetOwner());

		if (!bBlocked)
		{
			// Pose validated above: teleport-style move. Not "relevant" motion for velocity purposes
			FHitResult Hit(1.f);
			MoveRecord.LockRelevancy(false);
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, CandidateLocation - Location, CandidateQuat, false, Hit, ETeleportType::None, MoveRecord);
			MoveRecord.UnlockRelevancy();
			GSTN_WALL_LOG(2, TEXT("   rotate frac=%.2f angle=%.2fdeg shift=%s pivot=%s"),
				Fraction, FMath::RadiansToDegrees(FromQuat.AngularDistance(CandidateQuat)), *GSTNWallDebug::Vec(CandidateLocation - Location), *GSTNWallDebug::Vec(Pivot));
			return CandidateQuat;
		}
	}

	return FromQuat;
}

// ------------------------------------------------------------------------------------------------
// Sensing
// ------------------------------------------------------------------------------------------------

bool UGSTN_SimpleWallMovementMode::ProbeWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& Velocity,
	const FVector& ConcaveHitNormal, const FVector& ConcaveHitPoint, const FVector& GravityUp, float DeltaSeconds, FGSTN_WallProbeResult& OutResult) const
{
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	const UWorld* World = UpdatedComponent->GetWorld();

	float Radius, HalfHeight;
	GetCapsuleSize(UpdatedPrimitive, Radius, HalfHeight);
	const float ProbeRadius = Radius * ProbeRadiusScale;
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(ProbeRadius);
	const ECollisionChannel Channel = UpdatedPrimitive->GetCollisionObjectType();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GSTN_WallProbe), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams ResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);

	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector Up = Orientation.GetUpVector();
	const float CosMaxDeviation = FMath::Cos(FMath::DegreesToRadians(MaxProbeDeviationAngle));

	FVector NormalSum = FVector::ZeroVector;
	FVector PointSum = FVector::ZeroVector;
	float WeightSum = 0.f;
	float PointWeightSum = 0.f;

	auto AddSample = [&](const FVector& Normal, const FVector& Point, float Weight, bool bContributesToContact)
	{
		NormalSum += Normal * Weight;
		WeightSum += Weight;
		if (bContributesToContact)
		{
			PointSum += Point * Weight;
			PointWeightSum += Weight;
		}
	};

	// --- Along-wall probes (spatial filter) -------------------------------------------------
	// Sphere sweeps toward the wall. A sphere's Hit.Normal points from the contact to the sphere center,
	// so on edges it is "rounded" and changes continuously, unlike a line trace's face normal.
	const float ProbeReach = (Radius - ProbeRadius) + WallGap + WallProbeDistance;
	const float VerticalOffset = FMath::Max(HalfHeight - Radius, 0.f) * ProbeVerticalSpread;

	struct FProbeOrigin { FVector Offset; float Weight; };
	const FProbeOrigin Origins[] =
	{
		{ FVector::ZeroVector,     1.0f },
		{ Up * VerticalOffset,     0.5f },
		{ -Up * VerticalOffset,    0.5f },
	};

	for (const FProbeOrigin& Origin : Origins)
	{
		const FVector Start = Center + Origin.Offset;
		const FVector End = Start - WallNormal * ProbeReach;

		FHitResult Hit;
		const bool bProbeHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Channel, ProbeShape, QueryParams, ResponseParams);
		[[maybe_unused]] const int32 ProbeIdx = UE_PTRDIFF_TO_INT32(&Origin - Origins);
		GSTN_WALL_LOG(2, TEXT("   probe%d hit=%d pen=%d N=%s IN=%s P=%s t=%.2f actor=%s"),
			ProbeIdx, bProbeHit ? 1 : 0, Hit.bStartPenetrating ? 1 : 0, *GSTNWallDebug::Vec(Hit.Normal), *GSTNWallDebug::Vec(Hit.ImpactNormal), *GSTNWallDebug::Vec(Hit.ImpactPoint),
			Hit.Time, *GetNameSafe(Hit.GetActor()));
		if (!bProbeHit || Hit.bStartPenetrating)
		{
			continue;
		}

		const float Alignment = FVector::DotProduct(Hit.Normal, WallNormal);
		if (Alignment < CosMaxDeviation || !IsHitOnValidWall(World, Hit, GravityUp, Channel, QueryParams, ResponseParams))
		{
			GSTN_WALL_LOG(2, TEXT("   probe%d rejected (align=%.2f)"), ProbeIdx, Alignment);
			continue;
		}

		// Closer hits and hits agreeing with the current wall dominate. Agreement must reach zero exactly at the rejection
		// threshold: the probe direction follows the filtered normal, so a sample with a sizeable weight right at the cutoff
		// appears/disappears as the normal moves, snapping it back and forth (a self-sustained oscillation, even at rest)
		const float Proximity = 1.f - 0.5f * Hit.Time;
		const float Agreement = FMath::Square(FMath::Clamp((Alignment - CosMaxDeviation) / FMath::Max(1.f - CosMaxDeviation, UE_KINDA_SMALL_NUMBER), 0.f, 1.f));
		AddSample(Hit.Normal, Hit.ImpactPoint, Origin.Weight * Proximity * Agreement, true);

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug && IsInGameThread())
		{
			DrawDebugLine(World, Start, Hit.Location, FColor::Orange, false, -1.f, 0, 0.5f);
			DrawDebugPoint(World, Hit.ImpactPoint, 6.f, FColor::Orange, false, -1.f);
		}
#endif
	}

	const bool bHasAlongWallContact = PointWeightSum > 0.f;

	// --- Lookahead probe (concave corners ahead) ----------------------------------------------
	// Sweeps along the movement direction. Its weight ramps up as the obstacle gets closer, so the capsule starts
	// turning into an inside corner before touching it, instead of hitting it and snapping.
	const FVector MoveDir = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal();
	if (!MoveDir.IsNearlyZero())
	{
		const float CosMaxConcave = FMath::Cos(FMath::DegreesToRadians(MaxConcaveAngle));
		const float ExtentAlongMove = Radius + FMath::Max(HalfHeight - Radius, 0.f) * FMath::Abs(FVector::DotProduct(MoveDir, Up));
		const float LookaheadDistance = FMath::Max(MinLookaheadDistance, Velocity.Size() * LookaheadTime);
		const float ContactDistance = ExtentAlongMove - ProbeRadius; // probe travel at which the capsule would touch
		const FVector End = Center + MoveDir * (ContactDistance + LookaheadDistance);

		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Center, End, FQuat::Identity, Channel, ProbeShape, QueryParams, ResponseParams)
			&& !Hit.bStartPenetrating
			&& FVector::DotProduct(Hit.Normal, MoveDir) < -0.1f
			&& FVector::DotProduct(Hit.Normal, WallNormal) >= CosMaxConcave
			&& IsHitOnValidWall(World, Hit, GravityUp, Channel, QueryParams, ResponseParams)
			&& GSTNWall::CanFitOnWall(MovingComps, Orientation, Hit.Normal, Hit.ImpactPoint, Radius, HalfHeight, WallGap))
		{
			const float Closeness = 1.f - FMath::Clamp((Hit.Distance - ContactDistance) / LookaheadDistance, 0.f, 1.f);
			AddSample(Hit.Normal, Hit.ImpactPoint, 1.5f * FMath::Square(Closeness), !bHasAlongWallContact);

#if ENABLE_DRAW_DEBUG
			if (bDrawDebug && IsInGameThread())
			{
				DrawDebugLine(World, Center, Hit.Location, FColor::Magenta, false, -1.f, 0, 0.5f);
			}
#endif
		}
	}

	// --- Blocking hit from the move itself: we are physically in the inside corner ----------------
	if (!ConcaveHitNormal.IsNearlyZero())
	{
		AddSample(ConcaveHitNormal, ConcaveHitPoint, 2.f, !bHasAlongWallContact);
	}

	if (WeightSum <= UE_KINDA_SMALL_NUMBER || NormalSum.IsNearlyZero())
	{
		return false;
	}

	OutResult.Normal = NormalSum.GetSafeNormal();
	OutResult.ContactPoint = (PointWeightSum > 0.f) ? PointSum / PointWeightSum : Center - OutResult.Normal * (Radius + WallGap);
	OutResult.bValid = IsValidWallNormal(OutResult.Normal, GravityUp);
	return OutResult.bValid;
}

bool UGSTN_SimpleWallMovementMode::ProbeConvexWrap(const FMovingComponentSet& MovingComps, const FVector& WallNormal, const FVector& MoveDelta, const FVector& GravityUp, FGSTN_WallProbeResult& OutResult) const
{
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	const UWorld* World = UpdatedComponent->GetWorld();

	float Radius, HalfHeight;
	GetCapsuleSize(UpdatedPrimitive, Radius, HalfHeight);
	const float ProbeRadius = Radius * ProbeRadiusScale;
	const FCollisionShape ProbeShape = FCollisionShape::MakeSphere(ProbeRadius);
	const ECollisionChannel Channel = UpdatedPrimitive->GetCollisionObjectType();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GSTN_WallWrapProbe), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams ResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);

	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector MoveDir = MoveDelta.GetSafeNormal();
	const float CosMaxConvex = FMath::Cos(FMath::DegreesToRadians(MaxConvexAngle));

	auto Accept = [&](const FHitResult& Hit) -> bool
	{
		if (Hit.bStartPenetrating || FVector::DotProduct(Hit.Normal, WallNormal) < CosMaxConvex
			|| !IsHitOnValidWall(World, Hit, GravityUp, Channel, QueryParams, ResponseParams))
		{
			return false;
		}
		OutResult.Normal = Hit.Normal;
		OutResult.ContactPoint = Hit.ImpactPoint;
		OutResult.bValid = true;
		return true;
	};

	// Leg 1: dive "behind" the old wall plane, past where its surface used to be
	const FVector DiveEnd = Center - WallNormal * (Radius + WallGap + Radius);
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Center, DiveEnd, FQuat::Identity, Channel, ProbeShape, QueryParams, ResponseParams))
	{
		return Accept(Hit);
	}

	// Leg 2: sweep back against the movement direction to find the face around the edge.
	// The rounded sphere normal at the edge lets the orientation roll over it continuously.
	const FVector BackEnd = DiveEnd - MoveDir * (MoveDelta.Size() + 2.f * Radius);
	if (World->SweepSingleByChannel(Hit, DiveEnd, BackEnd, FQuat::Identity, Channel, ProbeShape, QueryParams, ResponseParams))
	{
#if ENABLE_DRAW_DEBUG
		if (bDrawDebug && IsInGameThread())
		{
			DrawDebugLine(World, Center, DiveEnd, FColor::Purple, false, -1.f, 0, 0.5f);
			DrawDebugLine(World, DiveEnd, Hit.Location, FColor::Purple, false, -1.f, 0, 0.5f);
		}
#endif
		return Accept(Hit);
	}

	return false;
}

void UGSTN_SimpleWallMovementMode::AdhereToWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp, float DeltaSeconds, FMovementRecord& MoveRecord) const
{
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	const UWorld* World = UpdatedComponent->GetWorld();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GSTN_WallAdhesion), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams ResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);

	// Sweep the real capsule toward the wall: it rests on the most protruding points, which bridges small crevices for free
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start - WallNormal * (WallGap + WallProbeDistance);

	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, End, Orientation, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams))
	{
		return;
	}

	if (Hit.bStartPenetrating)
	{
		// Already overlapping something, possibly not the wall itself (e.g. a slanted face behind an inside corner).
		// Resolve along the real penetration direction: pushing along the wall normal would not free the capsule and,
		// repeated every step, ratchets it away from the wall until it is lost
		MoveRecord.LockRelevancy(false);
		UMovementUtils::TryMoveToResolvePenetration(MovingComps, MOVECOMP_NoFlags, UMovementUtils::ComputePenetrationAdjustment(Hit), Hit, Orientation, MoveRecord);
		MoveRecord.UnlockRelevancy();
		return;
	}

	const float Error = Hit.Distance - WallGap; // > 0: too far, < 0: too close
	// Asymmetric: pushing out is instant (collision safety), pulling in is rate-limited (no pops on bumpy walls)
	const float Step = (Error > 0.f) ? FMath::Min(Error, MaxAdhesionSpeed * DeltaSeconds) : Error;
	const FVector Correction = -WallNormal * Step;

	if (Correction.IsNearlyZero(0.01f))
	{
		return;
	}

	FHitResult MoveHit(1.f);
	MoveRecord.LockRelevancy(false);
	UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, Correction, Orientation, true, MoveHit, ETeleportType::None, MoveRecord);
	MoveRecord.UnlockRelevancy();
}

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

void UGSTN_SimpleWallMovementMode::GetWallTangentBasis(const FQuat& Orientation, const FVector& WallNormal, FVector& OutTangentUp, FVector& OutTangentRight)
{
	OutTangentUp = FVector::VectorPlaneProject(Orientation.GetUpVector(), WallNormal).GetSafeNormal();
	if (OutTangentUp.IsNearlyZero())
	{
		// Capsule up is (nearly) along the normal, rebuild from its forward axis
		OutTangentUp = FVector::VectorPlaneProject(Orientation.GetForwardVector(), WallNormal).GetSafeNormal();
	}
	// Matches UE's basis convention (Y = Z ^ X) with X = -WallNormal
	OutTangentRight = FVector::CrossProduct(WallNormal, OutTangentUp);
}

void UGSTN_SimpleWallMovementMode::GetCapsuleSize(const UPrimitiveComponent* Primitive, float& OutRadius, float& OutHalfHeight) const
{
	const FCollisionShape Shape = Primitive ? Primitive->GetCollisionShape() : FCollisionShape();
	if (Shape.IsCapsule())
	{
		OutRadius = Shape.GetCapsuleRadius();
		OutHalfHeight = Shape.GetCapsuleHalfHeight();
	}
	else if (Shape.IsSphere())
	{
		OutRadius = OutHalfHeight = Shape.GetSphereRadius();
	}
	else
	{
		const FVector Extent = Shape.GetExtent();
		OutRadius = FMath::Max(FMath::Min(Extent.X, Extent.Y), 1.f);
		OutHalfHeight = FMath::Max(Extent.Z, OutRadius);
	}
}
