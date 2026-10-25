// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_WallMovementModeBase.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementModeBase)

UGSTN_WallMovementModeBase::UGSTN_WallMovementModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = false;
	NoWallModeName = DefaultModeNames::Falling;
	FloorModeName = DefaultModeNames::Walking;
}

void UGSTN_WallMovementModeBase::OnRegistered(const FName ModeName, const FMoverSimContext& SimContext)
{
	Super::OnRegistered(ModeName, SimContext);
	RegisteredModeName = ModeName;

	// Rollback-aware per-frame entries (entry handoff, ridden base); see GSTNWallBlackboard
	GSTNWallBlackboard::EnsureEntries(SimContext.Blackboard);

	// Mover drops every sync state struct that isn't persistent as soon as a mode that doesn't write it runs (Falling,
	// Walking...). The wall memory must outlive the wall mode, so the entry transition can read it in the air
	if (UMoverComponent* MoverComp = GetMoverComponent())
	{
		UScriptStruct* WallStateType = FGSTN_WallMovementSyncState::StaticStruct();
		if (!MoverComp->PersistentSyncStateDataTypes.ContainsByPredicate([WallStateType](const FMoverDataPersistence& Entry) { return Entry.RequiredType == WallStateType; }))
		{
			MoverComp->PersistentSyncStateDataTypes.Add(FMoverDataPersistence(WallStateType, true));
		}
	}
}

void UGSTN_WallMovementModeBase::Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	if (!OutSyncState)
	{
		return;
	}

	const UMoverComponent* MoverComp = GetMoverComponent();
	const FVector GravityUp = MoverComp ? MoverComp->GetUpDirection() : FVector::UpVector;

	// Starting wall: the one an entry transition found, else assume it is in front of the capsule until the first probe
	FVector WallNormal = FVector::ZeroVector;
	FVector WallPoint = FVector::ZeroVector;
	FGSTN_WallEntryInfo EntryInfo;
	if (SimContext.Blackboard.TryGet(GSTNWallBlackboard::EntryInfo, EntryInfo) && !EntryInfo.Normal.IsNearlyZero())
	{
		WallNormal = EntryInfo.Normal.GetSafeNormal();
		WallPoint = EntryInfo.Point;
	}
	// Consumed: these entries keep their last value across frames, so overwrite them with "none" (zero normal / no base)
	SimContext.Blackboard.TrySet(GSTNWallBlackboard::EntryInfo, FGSTN_WallEntryInfo());
	SimContext.Blackboard.TrySet(GSTNWallBlackboard::BaseInfo, FGSTN_WallBaseInfo());

	FMoverDefaultSyncState* DefaultSync = OutSyncState->SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (WallNormal.IsNearlyZero())
	{
		const FVector Forward = DefaultSync ? DefaultSync->GetOrientation_WorldSpace().Vector() : FVector::ForwardVector;
		WallNormal = -Forward;
	}

	// Entry momentum: split the incoming velocity on the starting wall's axes, scale and clamp each part
	if (DefaultSync)
	{
		const FGSTN_WallMovementPlane Plane = UGSTN_WallMovementUtils::MakeMovementPlane(DefaultSync->GetOrientation_WorldSpace().Quaternion(), WallNormal, GravityUp);
		const FVector InVelocity = DefaultSync->GetVelocity_WorldSpace();
		const FVector NewVelocity = UGSTN_WallMovementUtils::ComputeEntryVelocity(InVelocity, Plane, EntryMomentum);
		DefaultSync->SetTransforms_WorldSpace(DefaultSync->GetLocation_WorldSpace(), DefaultSync->GetOrientation_WorldSpace(),
			NewVelocity, DefaultSync->GetAngularVelocityDegrees_WorldSpace(), nullptr);

		GSTN_WALL_LOG(1, TEXT("ACTIVATE from %s wallN=%s inVel=%s -> vel=%s"), *PrevModeName.ToString(),
			*GSTNWallDebug::Vec(WallNormal), *GSTNWallDebug::Vec(InVelocity), *GSTNWallDebug::Vec(NewVelocity));
	}

	// Per-attachment state is reset; the wall memory (last wall, height cap) is kept on purpose
	FGSTN_WallMovementSyncState& WallState = OutSyncState->SyncStateCollection.FindOrAddMutableDataByType<FGSTN_WallMovementSyncState>();
	WallState.WallNormal = WallNormal;
	WallState.WallContactPoint = WallPoint;
	WallState.bHasWall = false;
	WallState.TimeWithoutWall = 0.f;
	WallState.bIsSkidding = false;
	WallState.PullAwayTime = 0.f;
	WallState.SurfaceSwitchTicks = 0;
}

// ------------------------------------------------------------------------------------------------
// GenerateMove: intent -> velocity in the wall plane
// ------------------------------------------------------------------------------------------------

void UGSTN_WallMovementModeBase::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	// The movement plane is the filtered wall normal, not the (lagging, smoothed) capsule orientation
	const FGSTN_WallMovementPlane Plane = MakeStartMovementPlane(StartState, StartingSyncState->GetOrientation_WorldSpace().Quaternion());
	const FVector WallIntent = ComputeWallIntent(StartState, Plane);

	// Prior velocity was already transported onto the wall at the end of last tick; project again in case the state came from elsewhere.
	// Plain projection (not length-preserving): a velocity coming from another mode, mostly into the wall, must not turn into full speed along it
	const FVector PriorVelocity = FVector::VectorPlaneProject(StartingSyncState->GetVelocity_WorldSpace(), Plane.Normal);

	OutProposedMove.LinearVelocity = GenerateWallMove(StartState, Plane, WallIntent, PriorVelocity, DeltaSeconds);
	OutProposedMove.DirectionIntent = WallIntent;
	OutProposedMove.bHasDirIntent = !WallIntent.IsNearlyZero();
	// Orientation on a wall is dictated by geometry, not by input: it's resolved in SimulationTick
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;

	GSTN_WALL_LOG(1, TEXT("GEN prior=%s intent=(h %.2f v %.2f) out=(h %.1f v %.1f)"), *GSTNWallDebug::Vec(PriorVelocity),
		Plane.GetHorizontalSpeed(WallIntent), Plane.GetVerticalSpeed(WallIntent),
		Plane.GetHorizontalSpeed(OutProposedMove.LinearVelocity), Plane.GetVerticalSpeed(OutProposedMove.LinearVelocity));
}

FGSTN_WallMovementPlane UGSTN_WallMovementModeBase::MakeStartMovementPlane(const FMoverTickStartData& StartState, const FQuat& FallbackOrientation) const
{
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FGSTN_WallMovementSyncState* WallState = StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();

	const FQuat Orientation = StartingSyncState ? StartingSyncState->GetOrientation_WorldSpace().Quaternion() : FallbackOrientation;
	return UGSTN_WallMovementUtils::MakeMovementPlane(Orientation, GetWallNormal(WallState, FallbackOrientation), GetMoverComponent()->GetUpDirection());
}

FVector UGSTN_WallMovementModeBase::ComputeWallIntent(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane) const
{
#if GSTN_WALL_DEBUG
	// gstn.Wall.DebugInput: drive the mode without a keyboard (see GSTN_WallMovementDebug.h)
	FVector DebugValue;
	if (GSTNWallDebug::GetInputOverride(DebugValue) == GSTNWallDebug::EInputOverride::World)
	{
		return FVector::VectorPlaneProject(DebugValue, Plane.Normal).GetClampedToMaxSize(1.f);
	}
#endif

	const FCharacterDefaultInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!Inputs)
	{
		return FVector::ZeroVector;
	}

	FVector MoveIntent = UMovementUtils::ComputeDirectionIntent(Inputs->GetMoveInput_WorldSpace(), Inputs->GetMoveInputType(), FMath::Max(Horizontal.MaxSpeed, 1.f));
#if GSTN_WALL_DEBUG
	GSTNWallDebug::OverrideMoveInput(Inputs->ControlRotation, Plane.GravityUp, MoveIntent);
#endif

	MoveIntent = MoveIntent.GetClampedToMaxSize(1.f);
	if (MoveIntent.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	return MapMoveIntentToWall(*Inputs, MoveIntent, Plane);
}

FVector UGSTN_WallMovementModeBase::MapMoveIntentToWall(const FCharacterDefaultInputs& Inputs, const FVector& MoveIntent, const FGSTN_WallMovementPlane& Plane) const
{
	return FVector::VectorPlaneProject(MoveIntent, Plane.Normal).GetClampedToMaxSize(1.f);
}

FVector UGSTN_WallMovementModeBase::GenerateWallMove(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent,
	const FVector& PriorVelocity, float DeltaSeconds) const
{
	// Split on the gravity-relative axes of the wall plane: each axis has its own speeds, friction and braking.
	// Terminal speeds last: absolute limits whatever the source
	const float HorizontalSpeed = UGSTN_WallMovementUtils::ClampHorizontalSpeed(
		ComputeHorizontalSpeed(StartState, Plane, WallIntent, Plane.GetHorizontalSpeed(PriorVelocity), DeltaSeconds), Horizontal);
	const float VerticalSpeed = UGSTN_WallMovementUtils::ClampVerticalSpeed(
		ComputeVerticalSpeed(Plane, WallIntent, Plane.GetVerticalSpeed(PriorVelocity), DeltaSeconds), GetVerticalMovement());

	return Plane.MakeVelocity(HorizontalSpeed, VerticalSpeed);
}

float UGSTN_WallMovementModeBase::ComputeHorizontalSpeed(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent,
	float HorizontalSpeed, float DeltaSeconds) const
{
	return UGSTN_WallMovementUtils::ComputeHorizontalSpeed(HorizontalSpeed, Plane.GetHorizontalSpeed(WallIntent), Horizontal, DeltaSeconds);
}

float UGSTN_WallMovementModeBase::ComputeVerticalSpeed(const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float VerticalSpeed, float DeltaSeconds) const
{
	return UGSTN_WallMovementUtils::ComputeVerticalSpeed(VerticalSpeed, Plane.GetVerticalSpeed(WallIntent), GetVerticalMovement(), DeltaSeconds);
}

FGSTN_WallVerticalMovement UGSTN_WallMovementModeBase::GetVerticalMovement() const
{
	FGSTN_WallVerticalMovement Movement;
	Movement.MaxUpSpeed = 0.f;
	Movement.MaxDownSpeed = 0.f;
	Movement.BrakingFriction = 0.f;
	Movement.BrakingDeceleration = 0.f;
	Movement.StopSpeed = 0.f;
	Movement.TerminalUpSpeed = 0.f;
	Movement.TerminalDownSpeed = 0.f;
	return Movement;
}

// ------------------------------------------------------------------------------------------------
// SimulationTick: move, sense, orient, adhere
// ------------------------------------------------------------------------------------------------

void UGSTN_WallMovementModeBase::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	UMoverComponent* MoverComp = GetMoverComponent();
	const FMovingComponentSet& MovingComps = Params.MovingComps;
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	if (!MoverComp || !UpdatedComponent || !MovingComps.UpdatedPrimitive.IsValid())
	{
		return;
	}

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(Params.TimeStep.StepMs * 0.001f);

	if (UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable())
	{
		// On a wall = no floor
		SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	const FQuat StartQuat = UpdatedComponent->GetComponentQuat();
	FGSTN_WallTickState TickState = MakeTickState(Params, StartQuat);
#if GSTN_WALL_DEBUG
	const FVector StartWallNormal = TickState.WallNormal;
#endif

	// Ride the dynamic wall: move/rotate with it before anything else, carrying the movement plane and the velocity along
	if (bUseMovementBases)
	{
		ApplyMovementBase(Params, TickState, MoveRecord);
	}

	// Facing away from the wall (e.g. grabbing it with the back): turn the heading first, the usual smoothing handles the tilt
	UGSTN_WallMovementUtils::TryMoveToFaceWall(MovingComps, TickState.WallNormal, MoveRecord);
	TickState.Orientation = UpdatedComponent->GetComponentQuat();

	// Held in place from outside (hitstop): no voluntary exit, and the timers stand still
	const bool bLocked = IsMovementLocked(Params.StartState);
	const float LockedTimeWithoutWall = TickState.TimeWithoutWall;
	const float LockedPullAwayTime = TickState.PullAwayTime;

	// Leaving on purpose (subclasses: wall jump, detach)
	if (!bLocked)
	{
		CheckForWallExit(Params, TickState);
	}
	const bool bLeftOnPurpose = TickState.IsLeaving();

	if (!bLeftOnPurpose)
	{
		MoveAlongWall(Params, TickState, MoveRecord);
		if (bLocked && !TickState.IsLeaving())
		{
			TickState.TimeWithoutWall = LockedTimeWithoutWall;
			TickState.PullAwayTime = LockedPullAwayTime;
		}
	}

	UpdateMovementBase(Params, TickState);
	CaptureFinalState(Params, StartQuat, TickState, bLeftOnPurpose, OutputState);

#if GSTN_WALL_DEBUG
	// Metrics CSV only (A/B tooling): the legacy tick is not instrumented otherwise and nothing here feeds back into the simulation
	if (TickState.IsLeaving())
	{
		GSTNWallDebug::NoteEvent(GSTNWallDebug::Ev_Exit);
	}
	GSTNWallDebug::FinishTick(RegisteredModeName, (Params.TimeStep.BaseSimTimeMs + Params.TimeStep.StepMs) * 0.001, TickState.Velocity, TickState.GravityUp,
		FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(StartWallNormal.GetSafeNormal() | TickState.WallNormal.GetSafeNormal()), -1.f, 1.f))),
		UpdatedComponent->GetComponentLocation(), GSTNWallDebug::MetricsSource(MoverComp));
#endif
#if ENABLE_DRAW_DEBUG
	DrawWallDebug(MovingComps, TickState);
#endif
}

FGSTN_WallTickState UGSTN_WallMovementModeBase::MakeTickState(const FSimulationTickParams& Params, const FQuat& StartQuat) const
{
	const FGSTN_WallMovementSyncState* StartWallState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();

	FGSTN_WallTickState State;
	State.WallNormal = GetWallNormal(StartWallState, StartQuat);
	State.WallContactPoint = StartWallState ? StartWallState->WallContactPoint : FVector::ZeroVector;
	State.bHasWall = StartWallState ? StartWallState->bHasWall : false;
	State.TimeWithoutWall = StartWallState ? StartWallState->TimeWithoutWall : 0.f;
	State.PullAwayTime = StartWallState ? StartWallState->PullAwayTime : 0.f;
	State.SurfaceSwitchTicks = StartWallState ? StartWallState->SurfaceSwitchTicks : 0;
	State.Velocity = Params.ProposedMove.LinearVelocity;
	State.Orientation = StartQuat;
	State.GravityUp = GetMoverComponent()->GetUpDirection();
	return State;
}

bool UGSTN_WallMovementModeBase::IsMovementLocked(const FMoverTickStartData& StartState) const
{
	static FGameplayTag FrozenTag;
	if (!FrozenTag.IsValid())
	{
		FrozenTag = FGameplayTag::RequestGameplayTag(TEXT("Combat.Mover.Frozen"), false);
	}
	const UMoverComponent* MoverComp = GetMoverComponent();
	return FrozenTag.IsValid() && MoverComp && MoverComp->HasGameplayTagInState(StartState.SyncState, FrozenTag, true);
}

void UGSTN_WallMovementModeBase::CheckForWallExit(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState) const
{
	// No voluntary exit: nothing to time either
	InOutState.PullAwayTime = 0.f;
}

void UGSTN_WallMovementModeBase::MoveAlongWall(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const
{
	const FMovingComponentSet& MovingComps = Params.MovingComps;
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	float CapsuleRadius, CapsuleHalfHeight;
	UGSTN_WallQueryUtils::GetCapsuleSize(MovingComps.UpdatedPrimitive.Get(), CapsuleRadius, CapsuleHalfHeight);

	// Substepping: corners must be sensed every few centimetres or fast movement skips past them
	const float SubstepLength = (MaxSubstepDistance > 0.f) ? MaxSubstepDistance : FMath::Max(CapsuleRadius * 0.5f, 1.f);
	const float TotalDistance = InOutState.Velocity.Size() * DeltaSeconds;
	const int32 NumSubsteps = FMath::Clamp(FMath::CeilToInt(TotalDistance / SubstepLength), 1, MaxSubsteps);
	const float SubDeltaSeconds = DeltaSeconds / NumSubsteps;

	const FGSTN_WallCheckSettings CheckSettings = GetWallCheckSettings();

	for (int32 SubstepIdx = 0; SubstepIdx < NumSubsteps; ++SubstepIdx)
	{
		const float RemainingSeconds = (NumSubsteps - SubstepIdx - 1) * SubDeltaSeconds;

		// 1. Translate along the wall tangent
		const FVector SubDelta = InOutState.Velocity * SubDeltaSeconds;
		FGSTN_WallCornerHit CornerHit;
		if (!MoveSubstep(MovingComps, SubDelta, SubDeltaSeconds, RemainingSeconds, CheckSettings, InOutState, CornerHit, MoveRecord))
		{
			break;
		}

		// 2-3. Sense the wall
		FGSTN_WallProbeResult Probe;
		if (!SenseWall(MovingComps, SubDelta, CornerHit, CheckSettings, InOutState, Probe))
		{
			InOutState.bHasWall = false;
			InOutState.TimeWithoutWall += SubDeltaSeconds;
			GSTN_WALL_LOG(1, TEXT("[%d/%d] NO WALL loc=%s vel=%s wallN=%s t=%.3f"), SubstepIdx, NumSubsteps,
				*GSTNWallDebug::Vec(MovingComps.UpdatedComponent->GetComponentLocation()), *GSTNWallDebug::Vec(InOutState.Velocity),
				*GSTNWallDebug::Vec(InOutState.WallNormal), InOutState.TimeWithoutWall);

			if (InOutState.TimeWithoutWall >= NoWallGraceTime && !NoWallModeName.IsNone())
			{
				InOutState.LeaveTo(NoWallModeName, RemainingSeconds);
				break;
			}
			continue;
		}

		// 4. Filter the movement plane toward the probe normal
		const FVector PrevWallNormal = InOutState.WallNormal;
		const bool bAtRest = !Params.ProposedMove.bHasDirIntent && InOutState.Velocity.SizeSquared() < FMath::Square(1.f);
		AttachToWall(Probe, bAtRest, SubDeltaSeconds, InOutState);

		// 5-6. Orientation
		OrientToWall(MovingComps, SubDelta.Size(), SubDeltaSeconds, InOutState, MoveRecord);

		// 7. Adhesion
		[[maybe_unused]] const FVector PreAdhereLoc = MovingComps.UpdatedComponent->GetComponentLocation();
		UGSTN_WallMovementUtils::TryMoveToAdhereToWall(MovingComps, InOutState.Orientation, InOutState.WallNormal, WallGap, WallProbeDistance,
			MaxAdhesionSpeed, SubDeltaSeconds, MoveRecord);

		GSTN_WALL_LOG(1, TEXT("[%d/%d] loc=%s rot=%s vel=%s probeN=%s wallN=%s concave=%d adhere=%s up=%s"), SubstepIdx, NumSubsteps,
			*GSTNWallDebug::Vec(MovingComps.UpdatedComponent->GetComponentLocation()), *InOutState.Orientation.Rotator().ToCompactString(),
			*GSTNWallDebug::Vec(InOutState.Velocity), *GSTNWallDebug::Vec(Probe.Normal), *GSTNWallDebug::Vec(InOutState.WallNormal), CornerHit.IsValid() ? 1 : 0,
			*GSTNWallDebug::Vec(MovingComps.UpdatedComponent->GetComponentLocation() - PreAdhereLoc), *GSTNWallDebug::Vec(InOutState.Orientation.GetUpVector()));

		// 8. Parallel-transport the velocity onto the new plane
		InOutState.Velocity = UGSTN_WallMovementUtils::TransportVelocity(InOutState.Velocity, PrevWallNormal, InOutState.WallNormal);
	}
}

bool UGSTN_WallMovementModeBase::MoveSubstep(const FMovingComponentSet& MovingComps, const FVector& SubDelta, float SubDeltaSeconds, float RemainingSeconds,
	const FGSTN_WallCheckSettings& CheckSettings, FGSTN_WallTickState& InOutState, FGSTN_WallCornerHit& OutCornerHit, FMovementRecord& MoveRecord) const
{
	if (SubDelta.IsNearlyZero())
	{
		return true;
	}

	FHitResult Hit(1.f);
	UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, SubDelta, InOutState.Orientation, true, Hit, ETeleportType::None, MoveRecord);
	if (!Hit.IsValidBlockingHit())
	{
		return true;
	}

	FMoverOnImpactParams ImpactParams(RegisteredModeName, Hit, SubDelta);
	GetMoverComponent()->HandleImpact(ImpactParams);

	// Moving into another climbable surface = inside corner, fed to the probes as a strong candidate.
	// Moving down into a surface flatter than a wall = landing on a floor
	const bool bIsConcaveCorner = UGSTN_WallQueryUtils::IsConcaveCornerHit(MovingComps, Hit, SubDelta, InOutState.Orientation, InOutState.WallNormal,
		InOutState.GravityUp, NormalDeadzoneAngle, CheckSettings);
	const bool bIsFloor = !bIsConcaveCorner && UGSTN_WallQueryUtils::IsFloorHit(Hit, SubDelta, InOutState.GravityUp, MinSurfaceAngle);

	if (bIsFloor && !FloorModeName.IsNone())
	{
		InOutState.LeaveTo(FloorModeName, (1.f - Hit.Time) * SubDeltaSeconds + RemainingSeconds);
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
		// Non-climbable obstacle (floor, too steep, etc.): stop pushing into it
		InOutState.Velocity = FVector::VectorPlaneProject(InOutState.Velocity, Hit.Normal);
	}

	UMovementUtils::TryMoveToSlideAlongSurface(MovingComps, SubDelta, 1.f - Hit.Time, InOutState.Orientation, Hit.Normal, Hit, true, MoveRecord);
	return true;
}

bool UGSTN_WallMovementModeBase::SenseWall(const FMovingComponentSet& MovingComps, const FVector& SubDelta, const FGSTN_WallCornerHit& CornerHit,
	const FGSTN_WallCheckSettings& CheckSettings, const FGSTN_WallTickState& State, FGSTN_WallProbeResult& OutProbe) const
{
	if (UGSTN_WallQueryUtils::FindWall(MovingComps, State.Orientation, State.WallNormal, State.Velocity, CornerHit, State.GravityUp, CheckSettings, OutProbe))
	{
		return true;
	}
	return !SubDelta.IsNearlyZero()
		&& UGSTN_WallQueryUtils::FindWallAroundConvexEdge(MovingComps, State.WallNormal, SubDelta, State.GravityUp, CheckSettings, OutProbe);
}

void UGSTN_WallMovementModeBase::AttachToWall(const FGSTN_WallProbeResult& Probe, bool bAtRest, float DeltaSeconds, FGSTN_WallTickState& InOutState) const
{
	// At rest the normal is frozen once the wall has been acquired: wedged between two surfaces (inside corners, props
	// against the wall) normal -> orientation/adhesion -> position -> probes is a feedback loop that can oscillate on its
	// own, and with no movement there is nothing new to sense anyway. Probes keep running to detect losing the wall.
	if (!(bAtRest && InOutState.bHasWall))
	{
		InOutState.WallNormal = UGSTN_WallMovementUtils::FilterWallNormal(InOutState.WallNormal, Probe.Normal, NormalDeadzoneAngle, MaxNormalChangeRate, DeltaSeconds);
	}
	InOutState.WallContactPoint = Probe.ContactPoint;
	InOutState.LastProbe = Probe;
	InOutState.bHasWall = true;
	InOutState.TimeWithoutWall = 0.f;
}

void UGSTN_WallMovementModeBase::OrientToWall(const FMovingComponentSet& MovingComps, float DistanceTravelled, float DeltaSeconds, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const
{
	const FQuat TargetQuat = ComputeTargetOrientation(InOutState.Orientation, InOutState.WallNormal, InOutState.GravityUp, InOutState.Velocity, DeltaSeconds);
	const FQuat SmoothedQuat = UGSTN_WallMovementUtils::SmoothOrientation(InOutState.Orientation, TargetQuat, RotationSharpness, RotationSmoothingDistance,
		MaxRotationRate, DistanceTravelled, DeltaSeconds);

	if (!InOutState.Orientation.Equals(SmoothedQuat, 1e-5f))
	{
		InOutState.Orientation = UGSTN_WallMovementUtils::TryMoveToRotateAroundPivot(MovingComps, InOutState.Orientation, SmoothedQuat, InOutState.WallContactPoint, MoveRecord);
	}
}

void UGSTN_WallMovementModeBase::CaptureFinalState(const FSimulationTickParams& Params, const FQuat& StartQuat, const FGSTN_WallTickState& State, bool bLeftOnPurpose, FMoverTickEndData& OutputState) const
{
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	const FGSTN_WallMovementSyncState* StartWallState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	FGSTN_WallMovementSyncState& OutputWallState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FGSTN_WallMovementSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();
	const FQuat FinalQuat = UpdatedComponent->GetComponentQuat();

	// Wall memory: the last wall and the height on it, read by entry transitions (re-entry rules, height cap)
	OutputWallState.bHasWallMemory = StartWallState && StartWallState->bHasWallMemory;
	OutputWallState.LastWallNormal = StartWallState ? StartWallState->LastWallNormal : FVector::ZeroVector;
	OutputWallState.LastWallHeight = StartWallState ? StartWallState->LastWallHeight : 0.f;
	OutputWallState.LastWallTimeMs = StartWallState ? StartWallState->LastWallTimeMs : 0.0;
	// Written by FGSTN_ResetWallMemoryEffect only; carried through every tick
	OutputWallState.MemoryResetTimeMs = StartWallState ? StartWallState->MemoryResetTimeMs : -1.0;
	if (State.bHasWall || bLeftOnPurpose)
	{
		OutputWallState.bHasWallMemory = true;
		OutputWallState.LastWallNormal = State.WallNormal;
		OutputWallState.LastWallHeight = FVector::DotProduct(FinalLocation, State.GravityUp);
		OutputWallState.LastWallTimeMs = Params.TimeStep.BaseSimTimeMs + Params.TimeStep.StepMs;
	}

	// Attachment
	OutputWallState.WallNormal = State.WallNormal;
	OutputWallState.WallContactPoint = State.WallContactPoint;
	OutputWallState.bHasWall = State.bHasWall;
	OutputWallState.TimeWithoutWall = State.TimeWithoutWall;
	OutputWallState.PullAwayTime = State.PullAwayTime;
	OutputWallState.bIsSkidding = false;
	// The surface-switch vote does not outlive a mode change (the mode that takes over starts counting from zero)
	OutputWallState.SurfaceSwitchTicks = State.IsLeaving() ? 0 : State.SurfaceSwitchTicks;

	// The intended (transported) velocity is used instead of the positional delta: the latter includes adhesion corrections and would jitter
	OutputSyncState.MoveDirectionIntent = Params.ProposedMove.bHasDirIntent ? Params.ProposedMove.DirectionIntent : FVector::ZeroVector;
	OutputSyncState.SetTransforms_WorldSpace(FinalLocation, FinalQuat.Rotator(), State.Velocity,
		UGSTN_WallMovementUtils::ComputeAngularVelocityDegrees(StartQuat, FinalQuat, DeltaSeconds), nullptr);
	UpdatedComponent->ComponentVelocity = State.Velocity;

	if (State.IsLeaving())
	{
		OutputState.MovementEndState.NextModeName = State.NextModeName;
		OutputState.MovementEndState.RemainingMs = FMath::Clamp(State.UnusedSeconds * 1000.f, 0.f, Params.TimeStep.StepMs);
		GSTN_WALL_LOG(1, TEXT("-> switching to %s (remaining %.1f ms)"),
			*State.NextModeName.ToString(), OutputState.MovementEndState.RemainingMs);
	}
}

// ------------------------------------------------------------------------------------------------
// Orientation
// ------------------------------------------------------------------------------------------------

FVector UGSTN_WallMovementModeBase::ComputeTargetUp(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const
{
	const FVector TransportedUp = UGSTN_WallMovementUtils::ComputeTransportedUp(CurrentQuat, WallNormal, GravityUp);
	return UGSTN_WallMovementUtils::ApplyUprightBias(TransportedUp, WallNormal, GravityUp, UprightSharpness, DeltaSeconds);
}

FQuat UGSTN_WallMovementModeBase::ComputeTargetOrientation(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const
{
	// X faces the wall, Z runs along it
	return FRotationMatrix::MakeFromXZ(-WallNormal, ComputeTargetUp(CurrentQuat, WallNormal, GravityUp, Velocity, DeltaSeconds)).ToQuat();
}

// ------------------------------------------------------------------------------------------------
// Movement bases
// ------------------------------------------------------------------------------------------------
// Mover's own based movement (ApplyMovementBaseChange + its based tick) forces the pawn upright by flattening its forward
// on the gravity plane, which would wreck the orientation on a wall. So the wall is ridden here instead, once per sim
// tick: the base is NOT put in FMoverDefaultSyncState (that would switch Mover's based tick on), it lives on the sim
// blackboard as the base transform plus the character's position in the base's frame.

void UGSTN_WallMovementModeBase::ApplyMovementBase(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const
{
	// Rollback blackboard: a re-simulation reads the base recorded by the frame before it, not the one the abandoned prediction left. But that record
	// is still THIS machine's prediction of that frame, and a correction has just replaced the location it recorded: bRideFromSyncedLocation takes
	// the character's place on the base from the synced start location instead (Mover's rollback put the component there), see 6.14
	FGSTN_WallBaseInfo BaseInfo;
	if (Params.SimContext.Blackboard.TryGet(GSTNWallBlackboard::BaseInfo, BaseInfo) && BaseInfo.Base.IsValid())
	{
		// The start sync state's location when there is one (it is what a correction writes), else the capsule's, which Mover keeps on it
		FVector RideFrom = FVector::ZeroVector;
		if (bRideFromSyncedLocation)
		{
			const FMoverDefaultSyncState* StartSync = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
			RideFrom = StartSync ? StartSync->GetLocation_WorldSpace() : (UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector);
		}
		UGSTN_WallMovementUtils::TryMoveWithWallBase(Params.MovingComps, BaseInfo, BaseRotation, InOutState.WallNormal, InOutState.Velocity, MoveRecord,
			bRideFromSyncedLocation ? &RideFrom : nullptr);
	}
}

void UGSTN_WallMovementModeBase::UpdateMovementBase(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState) const
{
	const USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent)
	{
		return;
	}

	// The entry is never "unset": no base is an empty FGSTN_WallBaseInfo (invalid Base)
	FGSTN_WallBaseInfo BaseInfo;
	if (InOutState.IsLeaving())
	{
		// Leaving: carry the moving wall's own velocity at the contact point, then forget the base
		FVector BaseVelocity;
		if (bUseMovementBases && bAddBaseVelocityOnExit && Params.SimContext.Blackboard.TryGet(GSTNWallBlackboard::BaseInfo, BaseInfo) && BaseInfo.Base.IsValid()
			&& UBasedMovementUtils::GetMovementBaseVelocityAtPoint(BaseInfo.Base.Get(), UpdatedComponent->GetComponentLocation(), BaseVelocity))
		{
			InOutState.Velocity += BaseVelocity;
		}
		Params.SimContext.Blackboard.TrySet(GSTNWallBlackboard::BaseInfo, FGSTN_WallBaseInfo());
		return;
	}

	// Staying: the wall component of the last probe is the base to ride next tick, if it is a dynamic one
	BaseInfo = FGSTN_WallBaseInfo();
	if (bUseMovementBases && InOutState.bHasWall)
	{
		UGSTN_WallMovementUtils::MakeWallBaseInfo(InOutState.LastProbe.Component.Get(), InOutState.LastProbe.BoneName, UpdatedComponent->GetComponentLocation(), BaseInfo);
	}
	Params.SimContext.Blackboard.TrySet(GSTNWallBlackboard::BaseInfo, BaseInfo);
}

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

FGSTN_WallCheckSettings UGSTN_WallMovementModeBase::GetWallCheckSettings() const
{
	FGSTN_WallCheckSettings Settings;
	Settings.MinSurfaceAngle = MinSurfaceAngle;
	Settings.MaxSurfaceAngle = MaxSurfaceAngle;
	Settings.WallGap = WallGap;
	Settings.WallProbeDistance = WallProbeDistance;
	Settings.ProbeRadiusScale = ProbeRadiusScale;
	Settings.ProbeVerticalSpread = ProbeVerticalSpread;
	Settings.MaxProbeDeviationAngle = MaxProbeDeviationAngle;
	Settings.MaxConcaveAngle = MaxConcaveAngle;
	Settings.MaxConvexAngle = MaxConvexAngle;
	Settings.LookaheadTime = LookaheadTime;
	Settings.MinLookaheadDistance = MinLookaheadDistance;
	Settings.SurfaceFilter = SurfaceFilter;
	Settings.bDrawDebug = bDrawDebug;
	return Settings;
}

FVector UGSTN_WallMovementModeBase::GetWallNormal(const FGSTN_WallMovementSyncState* WallState, const FQuat& FallbackOrientation)
{
	return (WallState && !WallState->WallNormal.IsNearlyZero()) ? WallState->WallNormal : -FallbackOrientation.GetForwardVector();
}

#if ENABLE_DRAW_DEBUG
void UGSTN_WallMovementModeBase::DrawWallDebug(const FMovingComponentSet& MovingComps, const FGSTN_WallTickState& State) const
{
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	if (!bDrawDebug || !IsInGameThread() || !UpdatedComponent)
	{
		return;
	}

	float CapsuleRadius, CapsuleHalfHeight;
	UGSTN_WallQueryUtils::GetCapsuleSize(MovingComps.UpdatedPrimitive.Get(), CapsuleRadius, CapsuleHalfHeight);

	const UWorld* World = UpdatedComponent->GetWorld();
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const FQuat Quat = UpdatedComponent->GetComponentQuat();
	DrawDebugCapsule(World, Loc, CapsuleHalfHeight, CapsuleRadius, Quat, State.bHasWall ? FColor::Green : FColor::Red, false, -1.f, 0, 0.5f);
	DrawDebugDirectionalArrow(World, State.WallContactPoint, State.WallContactPoint + State.WallNormal * 40.f, 8.f, FColor::Cyan, false, -1.f, 0, 1.f);
	DrawDebugDirectionalArrow(World, Loc, Loc + Quat.GetUpVector() * 50.f, 8.f, FColor::Blue, false, -1.f, 0, 1.f);
	DrawDebugDirectionalArrow(World, Loc, Loc + State.Velocity * 0.25f, 8.f, FColor::Yellow, false, -1.f, 0, 1.f);
}
#endif
