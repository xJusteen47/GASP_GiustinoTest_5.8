// Fill out your copyright notice in the Description page of Project Settings.


#include "GSTN_WallMovementModeBase.h"

#include "DrawDebugHelpers.h"
#include "GSTN_WallMovementDebug.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/NetSerialization.h"
#include "Engine/World.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementModeBase)

namespace GSTNWallBlackboard
{
	const FName EntryInfo = TEXT("GSTN_WallEntryInfo");
	const FName BaseInfo = TEXT("GSTN_WallBaseInfo");
	const FName MemoryResetTimeMs = TEXT("GSTN_WallMemoryResetTimeMs");
}

namespace GSTNWall
{
	/** Constant-rate move of a scalar toward a target, never overshooting */
	static float MoveTowards(float Value, float Target, float MaxStep)
	{
		return (Value < Target) ? FMath::Min(Value + MaxStep, Target) : FMath::Max(Value - MaxStep, Target);
	}

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
// FGSTN_WallInputs
// ------------------------------------------------------------------------------------------------

bool FGSTN_WallInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	uint8 bDetachBit = bWantsToDetach ? 1 : 0;
	Ar.SerializeBits(&bDetachBit, 1);
	bWantsToDetach = (bDetachBit != 0);

	bOutSuccess = true;
	return true;
}

void FGSTN_WallInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("WantsToDetach: %d\n", bWantsToDetach ? 1 : 0);
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

	uint8 Flags = (bHasWall ? 1 : 0) | (bIsSkidding ? 2 : 0) | (bHasWallMemory ? 4 : 0);
	Ar.SerializeBits(&Flags, 3);
	bHasWall = (Flags & 1) != 0;
	bIsSkidding = (Flags & 2) != 0;
	bHasWallMemory = (Flags & 4) != 0;

	SerializeFixedVector<1, 16>(WallNormal, Ar);
	SerializePackedVector<100, 30>(WallContactPoint, Ar);
	Ar << TimeWithoutWall;
	Ar << PullAwayTime;

	if (bHasWallMemory)
	{
		SerializeFixedVector<1, 16>(LastWallNormal, Ar);
		Ar << LastWallHeight;
		Ar << LastWallTimeMs;
	}

	bOutSuccess = true;
	return true;
}

void FGSTN_WallMovementSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("HasWall: %d  TimeWithoutWall: %.2f  Skidding: %d  PullAway: %.2f\n", bHasWall ? 1 : 0, TimeWithoutWall, bIsSkidding ? 1 : 0, PullAwayTime);
	Out.Appendf("WallNormal: X=%.3f Y=%.3f Z=%.3f\n", WallNormal.X, WallNormal.Y, WallNormal.Z);
	Out.Appendf("WallContact: X=%.2f Y=%.2f Z=%.2f\n", WallContactPoint.X, WallContactPoint.Y, WallContactPoint.Z);
	if (bHasWallMemory)
	{
		Out.Appendf("LastWall: N=(%.2f %.2f %.2f) H=%.1f T=%.0fms\n", LastWallNormal.X, LastWallNormal.Y, LastWallNormal.Z, LastWallHeight, LastWallTimeMs);
	}
}

bool FGSTN_WallMovementSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FGSTN_WallMovementSyncState& Authority = static_cast<const FGSTN_WallMovementSyncState&>(AuthorityState);
	constexpr float NormalCosTolerance = 0.996f; // ~5 degrees

	// Only compare normals while on a wall: off-wall both normals are zero and their dot product (0) would always mismatch.
	return bHasWall != Authority.bHasWall
		|| (bHasWall && FVector::DotProduct(WallNormal, Authority.WallNormal) < NormalCosTolerance);
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
	bIsSkidding = ToState.bIsSkidding;
	PullAwayTime = ToState.PullAwayTime;
	bHasWallMemory = ToState.bHasWallMemory;
	LastWallNormal = ToState.LastWallNormal;
	LastWallHeight = ToState.LastWallHeight;
	LastWallTimeMs = ToState.LastWallTimeMs;
}

// ------------------------------------------------------------------------------------------------
// UGSTN_WallMovementModeBase
// ------------------------------------------------------------------------------------------------

UGSTN_WallMovementModeBase::UGSTN_WallMovementModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = false;
	NoWallModeName = DefaultModeNames::Falling;
	FloorModeName = DefaultModeNames::Walking;
	Detach.DetachModeName = DefaultModeNames::Falling;
	WallJump.JumpModeName = DefaultModeNames::Falling;
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

void UGSTN_WallMovementModeBase::OnRegistered(const FName ModeName, const FMoverSimContext& SimContext)
{
	Super::OnRegistered(ModeName, SimContext);
	RegisteredModeName = ModeName;

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
	UMoverBlackboard* SimBlackboard = MoverComp ? MoverComp->GetSimBlackboard_Mutable() : nullptr;
	const FVector GravityUp = MoverComp ? MoverComp->GetUpDirection() : FVector::UpVector;

	// Starting wall: the one an entry transition found, else assume it is in front of the capsule until the first probe
	FVector WallNormal = FVector::ZeroVector;
	FVector WallPoint = FVector::ZeroVector;
	FGSTN_WallEntryInfo EntryInfo;
	if (SimBlackboard && SimBlackboard->TryGet(GSTNWallBlackboard::EntryInfo, EntryInfo) && !EntryInfo.Normal.IsNearlyZero())
	{
		WallNormal = EntryInfo.Normal.GetSafeNormal();
		WallPoint = EntryInfo.Point;
	}
	if (SimBlackboard)
	{
		SimBlackboard->Invalidate(GSTNWallBlackboard::EntryInfo);
		SimBlackboard->Invalidate(GSTNWallBlackboard::BaseInfo);
	}

	FMoverDefaultSyncState* DefaultSync = OutSyncState->SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	if (WallNormal.IsNearlyZero())
	{
		const FVector Forward = DefaultSync ? DefaultSync->GetOrientation_WorldSpace().Vector() : FVector::ForwardVector;
		WallNormal = -Forward;
	}

	// Entry momentum: split the incoming velocity on the starting wall's axes, scale and clamp each part
	if (DefaultSync)
	{
		const FQuat Orientation = DefaultSync->GetOrientation_WorldSpace().Quaternion();
		FVector HorizontalAxis, VerticalAxis;
		GetWallAxes(Orientation, WallNormal, GravityUp, HorizontalAxis, VerticalAxis);

		const FVector InVelocity = DefaultSync->GetVelocity_WorldSpace();
		const float HorizontalSpeed = FVector::DotProduct(InVelocity, HorizontalAxis) * EntryMomentum.HorizontalScale;
		float VerticalSpeed = FVector::DotProduct(InVelocity, VerticalAxis) * EntryMomentum.VerticalScale;
		if (EntryMomentum.MaxUpSpeed >= 0.f)
		{
			VerticalSpeed = FMath::Min(VerticalSpeed, EntryMomentum.MaxUpSpeed);
		}
		if (EntryMomentum.MaxDownSpeed >= 0.f)
		{
			VerticalSpeed = FMath::Max(VerticalSpeed, -EntryMomentum.MaxDownSpeed);
		}

		const FVector NewVelocity = HorizontalAxis * HorizontalSpeed + VerticalAxis * VerticalSpeed;
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
}

// ------------------------------------------------------------------------------------------------
// GenerateMove: intent -> velocity in the wall plane
// ------------------------------------------------------------------------------------------------

void UGSTN_WallMovementModeBase::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
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

	// Prior velocity was already transported onto the wall at the end of last tick; project again in case the state came from elsewhere.
	// Plain projection (not length-preserving): a velocity coming from another mode, mostly into the wall, must not turn into full speed along it
	const FVector PriorVelocity = FVector::VectorPlaneProject(StartingSyncState->GetVelocity_WorldSpace(), WallNormal);

	// Split everything on the gravity-relative axes of the wall plane: each axis has its own speeds, friction and braking
	FVector HorizontalAxis, VerticalAxis;
	GetWallAxes(Orientation, WallNormal, GravityUp, HorizontalAxis, VerticalAxis);

	float HorizontalSpeed = FVector::DotProduct(PriorVelocity, HorizontalAxis);
	float VerticalSpeed = FVector::DotProduct(PriorVelocity, VerticalAxis);
	const float HorizontalInput = FVector::DotProduct(WallIntent, HorizontalAxis);
	const float VerticalInput = FVector::DotProduct(WallIntent, VerticalAxis);

	// Horizontal: skid (hard brake against the movement) or the normal model
	const bool bWasSkidding = WallState && WallState->bIsSkidding;
	if (ComputeSkid(bWasSkidding, WallIntent, HorizontalSpeed, HorizontalAxis))
	{
		HorizontalSpeed = GSTNWall::MoveTowards(HorizontalSpeed, 0.f, Skid.Deceleration * DeltaSeconds);
	}
	else
	{
		HorizontalSpeed = UpdateAxisSpeed(HorizontalSpeed, HorizontalInput * Horizontal.MaxSpeed, Horizontal.Acceleration, Horizontal.Friction,
			Horizontal.BrakingFriction, Horizontal.BrakingDeceleration, Horizontal.StopSpeed, DeltaSeconds);
	}

	// Vertical: the subclass' model, then extra forces (gravity on a wall slide) after the stop snap so it never blocks a slide
	const FGSTN_WallVerticalMovement Vertical = GetVerticalMovement();
	const float VerticalTarget = VerticalInput * ((VerticalInput >= 0.f) ? Vertical.MaxUpSpeed : Vertical.MaxDownSpeed);
	VerticalSpeed = UpdateAxisSpeed(VerticalSpeed, VerticalTarget, Vertical.Acceleration, Vertical.Friction,
		Vertical.BrakingFriction, Vertical.BrakingDeceleration, Vertical.StopSpeed, DeltaSeconds);

	const float GravityAlongWall = FVector::DotProduct(MoverComp->GetGravityAcceleration(), VerticalAxis);
	VerticalSpeed += GetVerticalForce(VerticalSpeed, GravityAlongWall) * DeltaSeconds;

	// Terminal speeds: absolute limits whatever the source
	if (Horizontal.TerminalSpeed > 0.f)
	{
		HorizontalSpeed = FMath::Clamp(HorizontalSpeed, -Horizontal.TerminalSpeed, Horizontal.TerminalSpeed);
	}
	if (Vertical.TerminalUpSpeed > 0.f)
	{
		VerticalSpeed = FMath::Min(VerticalSpeed, Vertical.TerminalUpSpeed);
	}
	if (Vertical.TerminalDownSpeed > 0.f)
	{
		VerticalSpeed = FMath::Max(VerticalSpeed, -Vertical.TerminalDownSpeed);
	}

	OutProposedMove.LinearVelocity = HorizontalAxis * HorizontalSpeed + VerticalAxis * VerticalSpeed;

	GSTN_WALL_LOG(1, TEXT("GEN prior=%s intent=(h %.2f v %.2f) out=(h %.1f v %.1f) skid=%d gAlong=%.0f"),
		*GSTNWallDebug::Vec(PriorVelocity), HorizontalInput, VerticalInput, HorizontalSpeed, VerticalSpeed,
		ComputeSkid(bWasSkidding, WallIntent, FVector::DotProduct(PriorVelocity, HorizontalAxis), HorizontalAxis) ? 1 : 0, GravityAlongWall);
	OutProposedMove.DirectionIntent = WallIntent;
	OutProposedMove.bHasDirIntent = !WallIntent.IsNearlyZero();
	// Orientation on a wall is dictated by geometry, not by input: it's resolved in SimulationTick
	OutProposedMove.AngularVelocityDegrees = FVector::ZeroVector;
}

FVector UGSTN_WallMovementModeBase::MapInputToWall(const FCharacterDefaultInputs* Inputs, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp) const
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

	FVector Input = UMovementUtils::ComputeDirectionIntent(Inputs->GetMoveInput_WorldSpace(), Inputs->GetMoveInputType(), FMath::Max(Horizontal.MaxSpeed, 1.f));

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

void UGSTN_WallMovementModeBase::GetWallAxes(const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp, FVector& OutHorizontal, FVector& OutVertical) const
{
	// Vertical = gravity-up projected on the wall: the axis gravity slides along and the capsule keeps upright to.
	// On (near) ceilings it vanishes, so fall back to the capsule's own up on the surface
	const FVector GravityTangent = FVector::VectorPlaneProject(GravityUp, WallNormal);
	constexpr float MinVerticality = 0.1f;
	if (GravityTangent.SizeSquared() > FMath::Square(MinVerticality))
	{
		OutVertical = GravityTangent.GetSafeNormal();
	}
	else
	{
		FVector TangentRight;
		GetWallTangentBasis(Orientation, WallNormal, OutVertical, TangentRight);
	}
	// Same convention as the capsule tangent basis (right = normal ^ up)
	OutHorizontal = FVector::CrossProduct(WallNormal, OutVertical);
}

bool UGSTN_WallMovementModeBase::ComputeSkid(bool bWasSkidding, const FVector& Intent, float HorizontalSpeed, const FVector& HorizontalAxis) const
{
	if (!Skid.bEnableSkid)
	{
		return false;
	}

	const float Speed = FMath::Abs(HorizontalSpeed);
	const float InputStrength = Intent.Size();
	if (Speed <= UE_KINDA_SMALL_NUMBER || InputStrength < Skid.MinInputStrength)
	{
		return false;
	}

	// How much the stick opposes the horizontal movement: -1 = straight against it. A diagonal counts only for its opposing part
	const float Dot = (FVector::DotProduct(Intent, HorizontalAxis) / InputStrength) * FMath::Sign(HorizontalSpeed);

	return bWasSkidding
		? (Speed > Skid.EndSpeed && Dot <= Skid.InputDotThreshold + Skid.ExitDotMargin)
		: (Speed >= Skid.MinSpeed && Dot <= Skid.InputDotThreshold);
}

float UGSTN_WallMovementModeBase::UpdateAxisSpeed(float Speed, float TargetSpeed, float InAcceleration, float Friction, float BrakingFriction,
	float BrakingDeceleration, float StopSpeed, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return Speed;
	}

	if (FMath::Abs(TargetSpeed) > UE_KINDA_SMALL_NUMBER)
	{
		// With input (CharacterMovementComponent style): friction turns the velocity toward the input direction,
		// then acceleration brings it to the target speed. Above the target speed (e.g. entry momentum) it brakes back to it
		const float Direction = FMath::Sign(TargetSpeed);
		const float TargetMagnitude = FMath::Abs(TargetSpeed);

		Speed -= (Speed - Direction * FMath::Abs(Speed)) * FMath::Min(DeltaSeconds * Friction, 1.f);

		float SpeedAlong = Speed * Direction;
		if (SpeedAlong < TargetMagnitude)
		{
			SpeedAlong = FMath::Min(SpeedAlong + InAcceleration * DeltaSeconds, TargetMagnitude);
		}
		else
		{
			SpeedAlong = FMath::Max(SpeedAlong - (BrakingFriction * SpeedAlong + BrakingDeceleration) * DeltaSeconds, TargetMagnitude);
		}
		return SpeedAlong * Direction;
	}

	// No input: proportional + constant braking toward zero, then snap tiny residuals (proportional braking alone never reaches 0)
	const float Magnitude = FMath::Abs(Speed);
	const float Braked = FMath::Max(Magnitude - (BrakingFriction * Magnitude + BrakingDeceleration) * DeltaSeconds, 0.f);
	return (Braked < StopSpeed) ? 0.f : Braked * FMath::Sign(Speed);
}

bool UGSTN_WallMovementModeBase::IsValidWallNormal(const FVector& Normal, const FVector& GravityUp) const
{
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal, GravityUp), -1.f, 1.f)));
	return AngleDeg >= MinSurfaceAngle && AngleDeg <= MaxSurfaceAngle;
}

bool UGSTN_WallMovementModeBase::IsHitOnValidWall(const UWorld* World, const FHitResult& Hit, const FVector& GravityUp, ECollisionChannel Channel,
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

void UGSTN_WallMovementModeBase::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
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

	FVector WallNormal = (StartWallState && !StartWallState->WallNormal.IsNearlyZero()) ? StartWallState->WallNormal : -StartQuat.GetForwardVector();
	FVector WallContactPoint = StartWallState ? StartWallState->WallContactPoint : FVector::ZeroVector;
	bool bHasWall = StartWallState ? StartWallState->bHasWall : false;
	float TimeWithoutWall = StartWallState ? StartWallState->TimeWithoutWall : 0.f;
	float PullAwayTime = StartWallState ? StartWallState->PullAwayTime : 0.f;

	// Skid state, re-derived from the same start data GenerateMove used, so both agree
	bool bIsSkidding = false;
	{
		const FCharacterDefaultInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
		const FQuat SyncOrientation = StartingSyncState->GetOrientation_WorldSpace().Quaternion();
		FVector HorizontalAxis, VerticalAxis;
		GetWallAxes(SyncOrientation, WallNormal, GravityUp, HorizontalAxis, VerticalAxis);
		const FVector Intent = MapInputToWall(Inputs, SyncOrientation, WallNormal, GravityUp);
		const float PriorHorizontalSpeed = FVector::DotProduct(StartingSyncState->GetVelocity_WorldSpace(), HorizontalAxis);
		bIsSkidding = ComputeSkid(StartWallState && StartWallState->bIsSkidding, Intent, PriorHorizontalSpeed, HorizontalAxis);
	}

	FVector Velocity = Params.ProposedMove.LinearVelocity;
	OutputSyncState.MoveDirectionIntent = Params.ProposedMove.bHasDirIntent ? Params.ProposedMove.DirectionIntent : FVector::ZeroVector;

	// Ride the dynamic wall: move/rotate with it before anything else, carrying the movement plane and the velocity along
	if (bUseMovementBases)
	{
		ApplyMovementBase(MovingComps, WallNormal, Velocity, MoveRecord);
	}
	FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();

	// Facing away from the wall (e.g. grabbing it with the back): the target orientation would be a turn of more than 90 deg,
	// that the shortest-arc transport can take around any axis (the capsule tumbles) and that, applied around the wall
	// contact, swings the capsule through the wall. Turn the heading first, around the capsule's own axis and center: the
	// capsule is symmetric around it, so this never changes its collision. The usual smoothing handles what is left (tilt)
	{
		const FVector CapsuleUp = CurrentQuat.GetUpVector();
		const FVector Forward = FVector::VectorPlaneProject(CurrentQuat.GetForwardVector(), CapsuleUp).GetSafeNormal();
		const FVector TowardWall = FVector::VectorPlaneProject(-WallNormal, CapsuleUp).GetSafeNormal();
		if (!Forward.IsNearlyZero() && !TowardWall.IsNearlyZero() && FVector::DotProduct(Forward, TowardWall) < 0.f)
		{
			const float HeadingAngle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Forward, TowardWall), CapsuleUp),
				FVector::DotProduct(Forward, TowardWall));
			const FQuat HeadingQuat = (FQuat(CapsuleUp, HeadingAngle) * CurrentQuat).GetNormalized();

			FHitResult Hit(1.f);
			MoveRecord.LockRelevancy(false);
			UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, FVector::ZeroVector, HeadingQuat, false, Hit, ETeleportType::None, MoveRecord);
			MoveRecord.UnlockRelevancy();
			GSTN_WALL_LOG(1, TEXT("HEADING turn %.1fdeg (facing away from the wall)"), FMath::RadiansToDegrees(HeadingAngle));
			CurrentQuat = UpdatedComponent->GetComponentQuat();
		}
	}

	// Mode switch decided this tick, with the simulation time it leaves unused
	FName NextModeName = NAME_None;
	float UnusedSeconds = 0.f;

	// Leaving on purpose: wall jump, detach button, stick held away from the wall
	{
		const FCharacterDefaultInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();

		bool bJumpPressed = Inputs && Inputs->bIsJumpJustPressed;
#if GSTN_WALL_DEBUG
		bJumpPressed |= GSTNWallDebug::ConsumeJumpRequest();
#endif
		if (WallJump.bEnableWallJump && !WallJump.JumpModeName.IsNone() && bJumpPressed)
		{
			FVector HorizontalAxis, VerticalAxis;
			GetWallAxes(CurrentQuat, WallNormal, GravityUp, HorizontalAxis, VerticalAxis);
			Velocity = WallNormal * WallJump.AwaySpeed + GravityUp * WallJump.UpSpeed
				+ HorizontalAxis * FVector::DotProduct(Velocity, HorizontalAxis) * WallJump.HorizontalMomentumScale;
			NextModeName = WallJump.JumpModeName;
			UnusedSeconds = DeltaSeconds;
			GSTN_WALL_LOG(1, TEXT("WALL JUMP vel=%s"), *GSTNWallDebug::Vec(Velocity));
		}
		else if (Detach.bDetachByButton && !Detach.DetachModeName.IsNone() && IsDetachButtonPressed(StartState))
		{
			NextModeName = Detach.DetachModeName;
			UnusedSeconds = DeltaSeconds;
			GSTN_WALL_LOG(1, TEXT("DETACH (button)"));
		}
		else if (Detach.bDetachByPullingAway && !Detach.DetachModeName.IsNone() && Inputs)
		{
			// Raw stick direction in the world, flattened on the gravity plane, against the wall's horizontal outward direction
			FVector MoveInput = Inputs->GetMoveInput_WorldSpace();
#if GSTN_WALL_DEBUG
			FVector DebugValue;
			if (GSTNWallDebug::GetInputOverride(DebugValue) == GSTNWallDebug::EInputOverride::Stick)
			{
				const FVector CamForward = FVector::VectorPlaneProject(Inputs->ControlRotation.Vector(), GravityUp).GetSafeNormal();
				MoveInput = FVector::CrossProduct(GravityUp, CamForward) * DebugValue.X + CamForward * DebugValue.Y;
			}
#endif
			const FVector FlatInput = FVector::VectorPlaneProject(MoveInput, GravityUp).GetClampedToMaxSize(1.f);
			const FVector FlatNormal = FVector::VectorPlaneProject(WallNormal, GravityUp).GetSafeNormal();
			const bool bPullingAway = !FlatNormal.IsNearlyZero() && FVector::DotProduct(FlatInput, FlatNormal) >= Detach.PullAwayInputDot;

			PullAwayTime = bPullingAway ? PullAwayTime + DeltaSeconds : 0.f;
			if (bPullingAway && PullAwayTime >= Detach.PullAwayDelay)
			{
				NextModeName = Detach.DetachModeName;
				UnusedSeconds = DeltaSeconds;
				GSTN_WALL_LOG(1, TEXT("DETACH (pulled away %.2fs)"), PullAwayTime);
			}
		}
		else
		{
			PullAwayTime = 0.f;
		}
	}
	const bool bLeavingNow = !NextModeName.IsNone();

	float CapsuleRadius, CapsuleHalfHeight;
	GetCapsuleSize(UpdatedPrimitive, CapsuleRadius, CapsuleHalfHeight);

	// Substepping: corners must be sensed every few centimetres or fast movement skips past them
	const float SubstepLength = (MaxSubstepDistance > 0.f) ? MaxSubstepDistance : FMath::Max(CapsuleRadius * 0.5f, 1.f);
	const float TotalDistance = Velocity.Size() * DeltaSeconds;
	const int32 NumSubsteps = bLeavingNow ? 0 : FMath::Clamp(FMath::CeilToInt(TotalDistance / SubstepLength), 1, MaxSubsteps);
	const float SubDeltaSeconds = (NumSubsteps > 0) ? DeltaSeconds / NumSubsteps : 0.f;

	const UWorld* World = UpdatedComponent->GetWorld();
	const ECollisionChannel CollisionChannel = UpdatedPrimitive->GetCollisionObjectType();
	FCollisionQueryParams EdgeQueryParams(SCENE_QUERY_STAT(GSTN_WallEdgeCheck), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams EdgeResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, EdgeQueryParams, EdgeResponseParams);

	// Last successful probe: its component is the movement base candidate for next tick
	FGSTN_WallProbeResult LastProbe;

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
		LastProbe = Probe;
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

	// Movement base -----------------------------------------------------------------------------
	if (!NextModeName.IsNone())
	{
		// Leaving: carry the moving wall's own velocity at the contact point, then forget the base
		if (UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable())
		{
			FGSTN_WallBaseInfo BaseInfo;
			FVector BaseVelocity;
			if (bUseMovementBases && bAddBaseVelocityOnExit && SimBlackboard->TryGet(GSTNWallBlackboard::BaseInfo, BaseInfo) && BaseInfo.Base.IsValid()
				&& UBasedMovementUtils::GetMovementBaseVelocityAtPoint(BaseInfo.Base.Get(), UpdatedComponent->GetComponentLocation(), BaseVelocity))
			{
				Velocity += BaseVelocity;
			}
			SimBlackboard->Invalidate(GSTNWallBlackboard::BaseInfo);
		}
	}
	else
	{
		CaptureMovementBase(MovingComps, LastProbe, bUseMovementBases && bHasWall);
	}

	// Wall memory: the last wall and the height on it, read by entry transitions (re-entry rules, height cap) --------
	OutputWallState.bHasWallMemory = StartWallState && StartWallState->bHasWallMemory;
	OutputWallState.LastWallNormal = StartWallState ? StartWallState->LastWallNormal : FVector::ZeroVector;
	OutputWallState.LastWallHeight = StartWallState ? StartWallState->LastWallHeight : 0.f;
	OutputWallState.LastWallTimeMs = StartWallState ? StartWallState->LastWallTimeMs : 0.0;
	if (bHasWall || bLeavingNow)
	{
		OutputWallState.bHasWallMemory = true;
		OutputWallState.LastWallNormal = WallNormal;
		OutputWallState.LastWallHeight = FVector::DotProduct(UpdatedComponent->GetComponentLocation(), GravityUp);
		OutputWallState.LastWallTimeMs = Params.TimeStep.BaseSimTimeMs + Params.TimeStep.StepMs;
	}
	OutputWallState.bIsSkidding = bIsSkidding && NextModeName.IsNone();
	OutputWallState.PullAwayTime = PullAwayTime;

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

FQuat UGSTN_WallMovementModeBase::ComputeTargetOrientation(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const
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
	const float CeilingHeadingSharpness = GetCeilingHeadingSharpness();
	if (CeilingHeadingSharpness > 0.f && FVector::DotProduct(WallNormal, GravityUp) < 0.f)
	{
		const FVector MoveDir = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal();
		const float SpeedFactor = FMath::Clamp(Velocity.Size() / FMath::Max(Horizontal.MaxSpeed * 0.5f, 1.f), 0.f, 1.f);
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

FQuat UGSTN_WallMovementModeBase::ApplyRotationAroundPivot(const FMovingComponentSet& MovingComps, const FQuat& FromQuat, const FQuat& ToQuat, const FVector& Pivot, FMovementRecord& MoveRecord) const
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

bool UGSTN_WallMovementModeBase::ProbeWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& Velocity,
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
	float BestSampleWeight = 0.f;
	OutResult.Component = nullptr;
	OutResult.BoneName = NAME_None;

	auto AddSample =[&](const FVector& Normal, const FVector& Point, float Weight, bool bContributesToContact)
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
		const float SampleWeight = Origin.Weight * Proximity * Agreement;
		AddSample(Hit.Normal, Hit.ImpactPoint, SampleWeight, true);

		// The strongest along-wall sample names the wall component: the movement base candidate
		if (SampleWeight > BestSampleWeight)
		{
			BestSampleWeight = SampleWeight;
			OutResult.Component = Hit.GetComponent();
			OutResult.BoneName = Hit.BoneName;
		}

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

bool UGSTN_WallMovementModeBase::ProbeConvexWrap(const FMovingComponentSet& MovingComps, const FVector& WallNormal, const FVector& MoveDelta, const FVector& GravityUp, FGSTN_WallProbeResult& OutResult) const
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
		OutResult.Component = Hit.GetComponent();
		OutResult.BoneName = Hit.BoneName;
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

void UGSTN_WallMovementModeBase::AdhereToWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp, float DeltaSeconds, FMovementRecord& MoveRecord) const
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
// Movement bases
// ------------------------------------------------------------------------------------------------
// Mover's own based movement (ApplyMovementBaseChange + its based tick) forces the pawn upright by flattening its forward
// on the gravity plane, which would wreck the orientation on a wall. So the wall is ridden here instead, once per sim
// tick: the base is NOT put in FMoverDefaultSyncState (that would switch Mover's based tick on), it lives on the sim
// blackboard as the base transform plus the character's position in the base's frame.

void UGSTN_WallMovementModeBase::ApplyMovementBase(const FMovingComponentSet& MovingComps, FVector& InOutWallNormal, FVector& InOutVelocity, FMovementRecord& MoveRecord) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UMoverBlackboard* SimBlackboard = MoverComp ? MoverComp->GetSimBlackboard_Mutable() : nullptr;
	if (!UpdatedComponent || !SimBlackboard)
	{
		return;
	}

	FGSTN_WallBaseInfo BaseInfo;
	if (!SimBlackboard->TryGet(GSTNWallBlackboard::BaseInfo, BaseInfo) || !BaseInfo.Base.IsValid())
	{
		return;
	}

	FVector BaseLocation;
	FQuat BaseQuat;
	if (!UBasedMovementUtils::GetMovementBaseTransform(BaseInfo.Base.Get(), BaseInfo.BoneName, BaseLocation, BaseQuat))
	{
		return;
	}

	// Rigid motion of the base since last tick
	const FQuat DeltaRotation = BaseQuat * BaseInfo.BaseRotation.Inverse();
	const FVector TargetLocation = BaseLocation + BaseQuat.RotateVector(BaseInfo.LocalPosition);
	const FVector Delta = TargetLocation - UpdatedComponent->GetComponentLocation();
	if (Delta.IsNearlyZero(0.01f) && DeltaRotation.Equals(FQuat::Identity, 1e-6f))
	{
		return;
	}

	// The wall (movement plane) and the velocity relative to it rotate with the base, always
	InOutWallNormal = DeltaRotation.RotateVector(InOutWallNormal).GetSafeNormal();
	InOutVelocity = DeltaRotation.RotateVector(InOutVelocity);

	// The capsule follows according to BaseRotation
	const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();
	FQuat NewQuat = CurrentQuat;
	switch (BaseRotation)
	{
	case EGSTN_WallBaseRotation::Follow:
		NewQuat = DeltaRotation * CurrentQuat;
		break;
	case EGSTN_WallBaseRotation::FollowExceptRoll:
	{
		// Drop the twist around the (rotated) wall normal: the part that would roll the character over on the wall.
		// The swing still turns it with the wall; the upright bias then keeps it level to gravity
		FQuat Swing, Twist;
		DeltaRotation.ToSwingTwist(InOutWallNormal, Swing, Twist);
		NewQuat = Swing * CurrentQuat;
		break;
	}
	case EGSTN_WallBaseRotation::Ignore:
	default:
		break;
	}

	// Not swept: the pose is rigid relative to the wall, which has already moved. Anything else it overlaps is resolved by adhesion
	FHitResult Hit(1.f);
	MoveRecord.LockRelevancy(false);
	UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, Delta, NewQuat.GetNormalized(), false, Hit, ETeleportType::None, MoveRecord);
	MoveRecord.UnlockRelevancy();

	GSTN_WALL_LOG(1, TEXT("BASE %s delta=%s rot=%.2fdeg"), *GetNameSafe(BaseInfo.Base.Get()), *GSTNWallDebug::Vec(Delta),
		FMath::RadiansToDegrees(DeltaRotation.GetAngle()));
}

void UGSTN_WallMovementModeBase::CaptureMovementBase(const FMovingComponentSet& MovingComps, const FGSTN_WallProbeResult& Probe, bool bOnWall) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	UMoverBlackboard* SimBlackboard = MoverComp ? MoverComp->GetSimBlackboard_Mutable() : nullptr;
	if (!UpdatedComponent || !SimBlackboard)
	{
		return;
	}

	UPrimitiveComponent* Base = Probe.Component.Get();
	FVector BaseLocation;
	FQuat BaseQuat;
	if (!bOnWall || !Base || !UBasedMovementUtils::IsADynamicBase(Base)
		|| !UBasedMovementUtils::GetMovementBaseTransform(Base, Probe.BoneName, BaseLocation, BaseQuat))
	{
		SimBlackboard->Invalidate(GSTNWallBlackboard::BaseInfo);
		return;
	}

	FGSTN_WallBaseInfo BaseInfo;
	BaseInfo.Base = Base;
	BaseInfo.BoneName = Probe.BoneName;
	BaseInfo.BaseLocation = BaseLocation;
	BaseInfo.BaseRotation = BaseQuat;
	BaseInfo.LocalPosition = BaseQuat.UnrotateVector(UpdatedComponent->GetComponentLocation() - BaseLocation);
	SimBlackboard->Set(GSTNWallBlackboard::BaseInfo, BaseInfo);
}

bool UGSTN_WallMovementModeBase::IsDetachButtonPressed(const FMoverTickStartData& StartState) const
{
#if GSTN_WALL_DEBUG
	if (GSTNWallDebug::IsDetachHeld())
	{
		return true;
	}
#endif
	if (const FGSTN_WallInputs* WallInputs = StartState.InputCmd.InputCollection.FindDataByType<FGSTN_WallInputs>())
	{
		return WallInputs->bWantsToDetach;
	}

	// Fallback: the CharacterMover crouch intent. Read from the component, so it is not part of the predicted input
	if (Detach.bUseCrouchAsDetachButton)
	{
		if (const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent()))
		{
			return CharacterMover->GetCrouchIntent();
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------

void UGSTN_WallMovementModeBase::GetWallTangentBasis(const FQuat& Orientation, const FVector& WallNormal, FVector& OutTangentUp, FVector& OutTangentRight)
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

void UGSTN_WallMovementModeBase::GetCapsuleSize(const UPrimitiveComponent* Primitive, float& OutRadius, float& OutHalfHeight) const
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
