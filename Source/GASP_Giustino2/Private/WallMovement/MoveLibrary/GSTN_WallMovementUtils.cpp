// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallMovementUtils)

namespace GSTNWallMovement
{
	// Shrink applied to the capsule when validating a rotated pose, so resting contact with the wall doesn't count as overlap
	constexpr float OverlapTestShrink = 0.5f;

	/** Moves the updated component without counting it as "relevant" motion for velocity purposes (pose corrections) */
	static void SafeMoveIrrelevant(const FMovingComponentSet& MovingComps, const FVector& Delta, const FQuat& NewRotation, bool bSweep, FMovementRecord& MoveRecord)
	{
		FHitResult Hit(1.f);
		MoveRecord.LockRelevancy(false);
		UMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, Delta, NewRotation, bSweep, Hit, ETeleportType::None, MoveRecord);
		MoveRecord.UnlockRelevancy();
	}
}

// ------------------------------------------------------------------------------------------------
// Movement plane
// ------------------------------------------------------------------------------------------------

void UGSTN_WallMovementUtils::ComputeWallTangentBasis(const FQuat& Orientation, const FVector& WallNormal, FVector& OutTangentUp, FVector& OutTangentRight)
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

FGSTN_WallMovementPlane UGSTN_WallMovementUtils::MakeMovementPlane(const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp)
{
	FGSTN_WallMovementPlane Plane;
	Plane.Normal = WallNormal;
	Plane.GravityUp = GravityUp;
	Plane.Orientation = Orientation;

	// Vertical = gravity-up projected on the wall: the axis gravity slides along and the capsule keeps upright to.
	// On (near) ceilings it vanishes, so fall back to the capsule's own up on the surface
	const FVector GravityTangent = FVector::VectorPlaneProject(GravityUp, WallNormal);
	constexpr float MinVerticality = 0.1f;
	if (GravityTangent.SizeSquared() > FMath::Square(MinVerticality))
	{
		Plane.VerticalAxis = GravityTangent.GetSafeNormal();
	}
	else
	{
		FVector TangentRight;
		ComputeWallTangentBasis(Orientation, WallNormal, Plane.VerticalAxis, TangentRight);
	}
	// Same convention as the capsule tangent basis (right = normal ^ up)
	Plane.HorizontalAxis = FVector::CrossProduct(WallNormal, Plane.VerticalAxis);
	return Plane;
}

// ------------------------------------------------------------------------------------------------
// Velocity model
// ------------------------------------------------------------------------------------------------

float UGSTN_WallMovementUtils::ComputeAxisSpeed(float Speed, float TargetSpeed, float Acceleration, float Friction, float BrakingFriction,
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
			SpeedAlong = FMath::Min(SpeedAlong + Acceleration * DeltaSeconds, TargetMagnitude);
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

float UGSTN_WallMovementUtils::ComputeHorizontalSpeed(float Speed, float Input, const FGSTN_WallHorizontalMovement& Settings, float DeltaSeconds)
{
	return ComputeAxisSpeed(Speed, Input * Settings.MaxSpeed, Settings.Acceleration, Settings.Friction,
		Settings.BrakingFriction, Settings.BrakingDeceleration, Settings.StopSpeed, DeltaSeconds);
}

float UGSTN_WallMovementUtils::ComputeVerticalSpeed(float Speed, float Input, const FGSTN_WallVerticalMovement& Settings, float DeltaSeconds)
{
	const float TargetSpeed = Input * ((Input >= 0.f) ? Settings.MaxUpSpeed : Settings.MaxDownSpeed);
	return ComputeAxisSpeed(Speed, TargetSpeed, Settings.Acceleration, Settings.Friction,
		Settings.BrakingFriction, Settings.BrakingDeceleration, Settings.StopSpeed, DeltaSeconds);
}

float UGSTN_WallMovementUtils::ClampHorizontalSpeed(float Speed, const FGSTN_WallHorizontalMovement& Settings)
{
	return (Settings.TerminalSpeed > 0.f) ? FMath::Clamp(Speed, -Settings.TerminalSpeed, Settings.TerminalSpeed) : Speed;
}

float UGSTN_WallMovementUtils::ClampVerticalSpeed(float Speed, const FGSTN_WallVerticalMovement& Settings)
{
	if (Settings.TerminalUpSpeed > 0.f)
	{
		Speed = FMath::Min(Speed, Settings.TerminalUpSpeed);
	}
	if (Settings.TerminalDownSpeed > 0.f)
	{
		Speed = FMath::Max(Speed, -Settings.TerminalDownSpeed);
	}
	return Speed;
}

bool UGSTN_WallMovementUtils::IsSkidding(bool bWasSkidding, const FVector& Intent, float HorizontalSpeed, const FVector& HorizontalAxis, const FGSTN_WallSkid& Settings)
{
	if (!Settings.bEnableSkid)
	{
		return false;
	}

	const float Speed = FMath::Abs(HorizontalSpeed);
	const float InputStrength = Intent.Size();
	if (Speed <= UE_KINDA_SMALL_NUMBER || InputStrength < Settings.MinInputStrength)
	{
		return false;
	}

	// How much the stick opposes the horizontal movement: -1 = straight against it. A diagonal counts only for its opposing part
	const float Dot = (FVector::DotProduct(Intent, HorizontalAxis) / InputStrength) * FMath::Sign(HorizontalSpeed);

	return bWasSkidding
		? (Speed > Settings.EndSpeed && Dot <= Settings.InputDotThreshold + Settings.ExitDotMargin)
		: (Speed >= Settings.MinSpeed && Dot <= Settings.InputDotThreshold);
}

FVector UGSTN_WallMovementUtils::ComputeEntryVelocity(const FVector& InVelocity, const FGSTN_WallMovementPlane& Plane, const FGSTN_WallEntryMomentum& EntryMomentum)
{
	const float HorizontalSpeed = Plane.GetHorizontalSpeed(InVelocity) * EntryMomentum.HorizontalScale;
	float VerticalSpeed = Plane.GetVerticalSpeed(InVelocity) * EntryMomentum.VerticalScale;
	if (EntryMomentum.MaxUpSpeed >= 0.f)
	{
		VerticalSpeed = FMath::Min(VerticalSpeed, EntryMomentum.MaxUpSpeed);
	}
	if (EntryMomentum.MaxDownSpeed >= 0.f)
	{
		VerticalSpeed = FMath::Max(VerticalSpeed, -EntryMomentum.MaxDownSpeed);
	}
	return Plane.MakeVelocity(HorizontalSpeed, VerticalSpeed);
}

FVector UGSTN_WallMovementUtils::ComputeWallJumpVelocity(const FVector& Velocity, const FGSTN_WallMovementPlane& Plane, const FGSTN_WallJump& WallJump)
{
	return Plane.Normal * WallJump.AwaySpeed + Plane.GravityUp * WallJump.UpSpeed
		+ Plane.HorizontalAxis * Plane.GetHorizontalSpeed(Velocity) * WallJump.HorizontalMomentumScale;
}

bool UGSTN_WallMovementUtils::IsPullingAwayFromWall(const FVector& MoveInput, const FVector& WallNormal, const FVector& GravityUp, float MinInputDot)
{
	// Raw stick direction in the world, flattened on the gravity plane, against the wall's horizontal outward direction
	const FVector FlatInput = FVector::VectorPlaneProject(MoveInput, GravityUp).GetClampedToMaxSize(1.f);
	const FVector FlatNormal = FVector::VectorPlaneProject(WallNormal, GravityUp).GetSafeNormal();
	return !FlatNormal.IsNearlyZero() && FVector::DotProduct(FlatInput, FlatNormal) >= MinInputDot;
}

FVector UGSTN_WallMovementUtils::TransportVelocity(const FVector& Velocity, const FVector& FromNormal, const FVector& ToNormal)
{
	if (Velocity.IsNearlyZero())
	{
		return Velocity;
	}
	const FVector Transported = FQuat::FindBetweenNormals(FromNormal, ToNormal).RotateVector(Velocity);
	return ProjectOnPlaneKeepLength(Transported, ToNormal);
}

// ------------------------------------------------------------------------------------------------
// Normal filter and orientation
// ------------------------------------------------------------------------------------------------

FVector UGSTN_WallMovementUtils::FilterWallNormal(const FVector& CurrentNormal, const FVector& ProbeNormal, float DeadzoneAngle, float MaxChangeRate, float DeltaSeconds)
{
	const float ChangeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ProbeNormal, CurrentNormal), -1.f, 1.f)));
	float Blend = (DeadzoneAngle > 0.f) ? FMath::Square(FMath::Clamp(ChangeDeg / DeadzoneAngle, 0.f, 1.f)) : 1.f;
	const float MaxStepDeg = MaxChangeRate * DeltaSeconds;
	if (MaxChangeRate > 0.f && ChangeDeg * Blend > MaxStepDeg)
	{
		Blend = MaxStepDeg / ChangeDeg;
	}
	const FQuat NormalDelta = FQuat::FindBetweenNormals(CurrentNormal, ProbeNormal);
	return FQuat::Slerp(FQuat::Identity, NormalDelta, Blend).RotateVector(CurrentNormal).GetSafeNormal();
}

FVector UGSTN_WallMovementUtils::ComputeTransportedUp(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp)
{
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
	return Up;
}

FVector UGSTN_WallMovementUtils::ApplyUprightBias(const FVector& Up, const FVector& WallNormal, const FVector& GravityUp, float UprightSharpness, float DeltaSeconds)
{
	const FVector GravityTangent = FVector::VectorPlaneProject(GravityUp, WallNormal);
	const float Verticality = GravityTangent.Size(); // 1 = vertical wall, 0 = ceiling/floor
	if (Verticality <= UE_KINDA_SMALL_NUMBER || UprightSharpness <= 0.f)
	{
		return Up;
	}

	const float UprightAlpha = ExpAlpha(UprightSharpness * FMath::Square(Verticality), DeltaSeconds);
	return RotateTowardsInPlane(Up, GravityTangent / Verticality, WallNormal, UprightAlpha);
}

FQuat UGSTN_WallMovementUtils::SmoothOrientation(const FQuat& CurrentQuat, const FQuat& TargetQuat, float Sharpness, float SmoothingDistance,
	float MaxRotationRate, float DistanceTravelled, float DeltaSeconds)
{
	const float TimeAlpha = ExpAlpha(Sharpness, DeltaSeconds);
	const float DistAlpha = ExpAlpha(1.f / SmoothingDistance, DistanceTravelled);
	FQuat SmoothedQuat = FQuat::Slerp(CurrentQuat, TargetQuat, FMath::Max(TimeAlpha, DistAlpha)).GetNormalized();

	const float StepAngle = CurrentQuat.AngularDistance(SmoothedQuat);
	const float MaxStepAngle = FMath::DegreesToRadians(MaxRotationRate) * DeltaSeconds;
	if (StepAngle > MaxStepAngle && StepAngle > UE_KINDA_SMALL_NUMBER)
	{
		SmoothedQuat = FQuat::Slerp(CurrentQuat, SmoothedQuat, MaxStepAngle / StepAngle).GetNormalized();
	}
	return SmoothedQuat;
}

FVector UGSTN_WallMovementUtils::ComputeAngularVelocityDegrees(const FQuat& FromQuat, const FQuat& ToQuat, float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return FVector::ZeroVector;
	}

	FQuat DeltaQuat = ToQuat * FromQuat.Inverse();
	DeltaQuat.EnforceShortestArcWith(FQuat::Identity);
	FVector Axis;
	float AngleRad;
	DeltaQuat.ToAxisAndAngle(Axis, AngleRad);
	return Axis * FMath::RadiansToDegrees(AngleRad) / DeltaSeconds;
}

// ------------------------------------------------------------------------------------------------
// Moves
// ------------------------------------------------------------------------------------------------

bool UGSTN_WallMovementUtils::TryMoveToFaceWall(const FMovingComponentSet& MovingComps, const FVector& WallNormal, FMovementRecord& MoveRecord)
{
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent)
	{
		return false;
	}

	// The target orientation would be a turn of more than 90 deg, that the shortest-arc transport can take around any axis
	// (the capsule tumbles) and that, applied around the wall contact, swings the capsule through the wall. The usual
	// smoothing handles what is left (tilt)
	const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();
	const FVector CapsuleUp = CurrentQuat.GetUpVector();
	const FVector Forward = FVector::VectorPlaneProject(CurrentQuat.GetForwardVector(), CapsuleUp).GetSafeNormal();
	const FVector TowardWall = FVector::VectorPlaneProject(-WallNormal, CapsuleUp).GetSafeNormal();
	if (Forward.IsNearlyZero() || TowardWall.IsNearlyZero() || FVector::DotProduct(Forward, TowardWall) >= 0.f)
	{
		return false;
	}

	const float HeadingAngle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Forward, TowardWall), CapsuleUp),
		FVector::DotProduct(Forward, TowardWall));
	const FQuat HeadingQuat = (FQuat(CapsuleUp, HeadingAngle) * CurrentQuat).GetNormalized();

	GSTNWallMovement::SafeMoveIrrelevant(MovingComps, FVector::ZeroVector, HeadingQuat, false, MoveRecord);
	GSTN_WALL_LOG(1, TEXT("HEADING turn %.1fdeg (facing away from the wall)"), FMath::RadiansToDegrees(HeadingAngle));
	return true;
}

FQuat UGSTN_WallMovementUtils::TryMoveToRotateAroundPivot(const FMovingComponentSet& MovingComps, const FQuat& FromQuat, const FQuat& ToQuat, const FVector& Pivot, FMovementRecord& MoveRecord)
{
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	if (!UpdatedComponent || !UpdatedPrimitive)
	{
		return FromQuat;
	}

	float Radius, HalfHeight;
	UGSTN_WallQueryUtils::GetCapsuleSize(UpdatedPrimitive, Radius, HalfHeight);
	const FCollisionShape TestShape = FCollisionShape::MakeCapsule(
		FMath::Max(Radius - GSTNWallMovement::OverlapTestShrink, 1.f),
		FMath::Max(HalfHeight - GSTNWallMovement::OverlapTestShrink, 1.f));

	const FVector Location = UpdatedComponent->GetComponentLocation();

	// If the pose overlaps, back off the rotation
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
			// Pose validated above: teleport-style move
			GSTNWallMovement::SafeMoveIrrelevant(MovingComps, CandidateLocation - Location, CandidateQuat, false, MoveRecord);
			GSTN_WALL_LOG(2, TEXT("   rotate frac=%.2f angle=%.2fdeg shift=%s pivot=%s"),
				Fraction, FMath::RadiansToDegrees(FromQuat.AngularDistance(CandidateQuat)), *GSTNWallDebug::Vec(CandidateLocation - Location), *GSTNWallDebug::Vec(Pivot));
			return CandidateQuat;
		}
	}

	return FromQuat;
}

void UGSTN_WallMovementUtils::TryMoveToAdhereToWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, float WallGap,
	float WallProbeDistance, float MaxAdhesionSpeed, float DeltaSeconds, FMovementRecord& MoveRecord)
{
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	const UWorld* World = UpdatedComponent ? UpdatedComponent->GetWorld() : nullptr;
	if (!World || !UpdatedPrimitive)
	{
		return;
	}

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

	GSTNWallMovement::SafeMoveIrrelevant(MovingComps, Correction, Orientation, true, MoveRecord);
}

bool UGSTN_WallMovementUtils::AdhereAndSense(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp,
	const FGSTN_WallCheckSettings& Settings, float MaxAdhesionSpeed, float DeltaSeconds, FMovementRecord& MoveRecord, FHitResult& OutContact)
{
	OutContact = FHitResult(1.f);

	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();
	const UWorld* World = UpdatedComponent ? UpdatedComponent->GetWorld() : nullptr;
	if (!World || !UpdatedPrimitive)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GSTN_WallContactAdhere), false, UpdatedComponent->GetOwner());
	FCollisionResponseParams ResponseParams;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParams);
	// The contact sweep must not stop on another pawn standing between the capsule and the wall, nor adopt it as the wall
	UGSTN_WallQueryUtils::ApplyPawnFilter(Settings, ResponseParams);

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start - WallNormal * (Settings.WallGap + Settings.WallProbeDistance);

	FHitResult Hit;
	{
		GSTN_WALL_QUERY_SCOPE();
		if (!World->SweepSingleByChannel(Hit, Start, End, Orientation, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), QueryParams, ResponseParams))
		{
			return false;
		}
	}
	OutContact = Hit;

	if (Hit.bStartPenetrating)
	{
		// Same rule as TryMoveToAdhereToWall: resolve along the real penetration direction, never along the wall normal
		MoveRecord.LockRelevancy(false);
		UMovementUtils::TryMoveToResolvePenetration(MovingComps, MOVECOMP_NoFlags, UMovementUtils::ComputePenetrationAdjustment(Hit), Hit, Orientation, MoveRecord);
		MoveRecord.UnlockRelevancy();
		return false;
	}

	// IsHitOnValidWall only traces on an edge contact (rounded normal): count that one too
	constexpr float EdgeCosThreshold = 0.9994f;
	if (FVector::DotProduct(Hit.Normal, Hit.ImpactNormal) < EdgeCosThreshold)
	{
		GSTN_WALL_QUERY_SCOPE();
	}
	if (!UGSTN_WallQueryUtils::IsHitOnValidWall(World, Hit, GravityUp, Settings, UpdatedPrimitive->GetCollisionObjectType(), QueryParams, ResponseParams))
	{
		return false;
	}

	const float Error = Hit.Distance - Settings.WallGap; // > 0: too far, < 0: too close
	if (Error > 0.f)
	{
		// The sweep proved the way to the wall free up to Hit.Distance: no need to sweep again for the pull-in
		const float Step = FMath::Min(Error, MaxAdhesionSpeed * DeltaSeconds);
		if (Step > 0.01f)
		{
			GSTNWallMovement::SafeMoveIrrelevant(MovingComps, -WallNormal * Step, Orientation, false, MoveRecord);
		}
	}
	else if (Error < -0.01f)
	{
		GSTN_WALL_QUERY_SCOPE();
		GSTNWallMovement::SafeMoveIrrelevant(MovingComps, -WallNormal * Error, Orientation, true, MoveRecord);
	}
	return true;
}

bool UGSTN_WallMovementUtils::TryMoveWithWallBase(const FMovingComponentSet& MovingComps, const FGSTN_WallBaseInfo& BaseInfo, EGSTN_WallBaseRotation BaseRotation,
	FVector& InOutWallNormal, FVector& InOutVelocity, FMovementRecord& MoveRecord, const FVector* RideFrom)
{
	const USceneComponent* UpdatedComponent = MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent || !BaseInfo.Base.IsValid())
	{
		return false;
	}

	FVector BaseLocation;
	FQuat BaseQuat;
	if (!UBasedMovementUtils::GetMovementBaseTransform(BaseInfo.Base.Get(), BaseInfo.BoneName, BaseLocation, BaseQuat))
	{
		return false;
	}

	// Rigid motion of the base since last tick. The character's place in the base's frame is either the recorded one or, rollback-safe, the one of
	// its synced start location in the base's PREVIOUS frame: after a correction only the latter agrees with the authority (wall-slide-v2.md 6.14)
	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
	const FVector LocalPosition = RideFrom ? BaseInfo.BaseRotation.UnrotateVector(*RideFrom - BaseInfo.BaseLocation) : BaseInfo.LocalPosition;
	const FQuat DeltaRotation = BaseQuat * BaseInfo.BaseRotation.Inverse();
	const FVector TargetLocation = BaseLocation + BaseQuat.RotateVector(LocalPosition);
	const FVector Delta = TargetLocation - CurrentLocation;
	if (Delta.IsNearlyZero(0.01f) && DeltaRotation.Equals(FQuat::Identity, 1e-6f))
	{
		return false;
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
	GSTNWallMovement::SafeMoveIrrelevant(MovingComps, Delta, NewQuat.GetNormalized(), false, MoveRecord);

	GSTN_WALL_LOG(1, TEXT("BASE %s delta=%s rot=%.2fdeg"), *GetNameSafe(BaseInfo.Base.Get()), *GSTNWallDebug::Vec(Delta),
		FMath::RadiansToDegrees(DeltaRotation.GetAngle()));
	return true;
}

bool UGSTN_WallMovementUtils::MakeWallBaseInfo(UPrimitiveComponent* Base, FName BoneName, const FVector& CharacterLocation, FGSTN_WallBaseInfo& OutBaseInfo)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	if (!Base || !UBasedMovementUtils::IsADynamicBase(Base) || !UBasedMovementUtils::GetMovementBaseTransform(Base, BoneName, BaseLocation, BaseQuat))
	{
		return false;
	}

	OutBaseInfo.Base = Base;
	OutBaseInfo.BoneName = BoneName;
	OutBaseInfo.BaseLocation = BaseLocation;
	OutBaseInfo.BaseRotation = BaseQuat;
	OutBaseInfo.LocalPosition = BaseQuat.UnrotateVector(CharacterLocation - BaseLocation);
	return true;
}

// ------------------------------------------------------------------------------------------------
// Math helpers
// ------------------------------------------------------------------------------------------------

float UGSTN_WallMovementUtils::MoveTowards(float Value, float Target, float MaxStep)
{
	return (Value < Target) ? FMath::Min(Value + MaxStep, Target) : FMath::Max(Value - MaxStep, Target);
}

FVector UGSTN_WallMovementUtils::RotateTowardsInPlane(const FVector& A, const FVector& B, const FVector& Axis, float Alpha)
{
	const float Angle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(A, B), Axis), FVector::DotProduct(A, B));
	return FQuat(Axis, Angle * Alpha).RotateVector(A);
}

FVector UGSTN_WallMovementUtils::ProjectOnPlaneKeepLength(const FVector& V, const FVector& PlaneNormal)
{
	const float Length = V.Size();
	const FVector Projected = FVector::VectorPlaneProject(V, PlaneNormal);
	return Projected.GetSafeNormal() * Length;
}

float UGSTN_WallMovementUtils::ExpAlpha(float Sharpness, float Step)
{
	return 1.f - FMath::Exp(-Sharpness * Step);
}
