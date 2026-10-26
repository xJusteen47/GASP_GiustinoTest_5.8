// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HitMathUtils.h"

namespace
{
	/** A direction on the plane normal to the unit vector Up that depends only on Up, for the degenerate cases */
	FVector FixedPerpendicular(const FVector& Up)
	{
		const double AbsX = FMath::Abs(Up.X), AbsY = FMath::Abs(Up.Y), AbsZ = FMath::Abs(Up.Z);
		const FVector Axis = (AbsX <= AbsY && AbsX <= AbsZ) ? FVector::XAxisVector : (AbsY <= AbsZ ? FVector::YAxisVector : FVector::ZAxisVector);
		return (Axis - Up * FVector::DotProduct(Axis, Up)).GetSafeNormal();
	}

	/** The horizontal unit direction of the first candidate that has one */
	FVector FirstHorizontal(const FVector& Up, const FVector& A, const FVector& B, const FVector& C)
	{
		for (const FVector* Candidate : { &A, &B, &C })
		{
			const FVector Projected = UGSTN_HitMathUtils::ProjectOnPlane(*Candidate, Up).GetSafeNormal();
			if (!Projected.IsZero())
			{
				return Projected;
			}
		}
		return FixedPerpendicular(Up);
	}
}

FVector UGSTN_HitMathUtils::ProjectOnPlane(const FVector& V, const FVector& Up)
{
	const FVector UnitUp = Up.GetSafeNormal();
	return UnitUp.IsZero() ? V : V - UnitUp * FVector::DotProduct(V, UnitUp);
}

FVector UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource YawSource, float AngleDeg, EGSTN_SpecialAngle SpecialAngle, const FVector& Up, const FVector& AttackerFacing, const FVector& HitboxToVictim, const FVector& HitboxVelocity, bool bVictimGrounded, float Knockback, const FGSTN_SakuraiParams& Sakurai)
{
	const FVector UnitUp = Up.GetSafeNormal();
	if (UnitUp.IsZero())
	{
		return FVector::ZeroVector;
	}

	if (SpecialAngle == EGSTN_SpecialAngle::AutoLink)
	{
		const FVector Along = HitboxVelocity.GetSafeNormal();
		if (!Along.IsZero())
		{
			return Along;
		}
	}

	const FVector& Preferred = YawSource == EGSTN_YawSource::HitboxToVictim ? HitboxToVictim
		: YawSource == EGSTN_YawSource::HitboxVelocity ? HitboxVelocity : AttackerFacing;
	const FVector Horizontal = FirstHorizontal(UnitUp, Preferred, AttackerFacing, HitboxToVictim);

	double Angle = AngleDeg;
	if (SpecialAngle == EGSTN_SpecialAngle::Sakurai3D)
	{
		Angle = (bVictimGrounded && Knockback < Sakurai.KnockbackThreshold) ? Sakurai.GroundedLowAngleDeg : Sakurai.HighAngleDeg;
	}
	const double Radians = FMath::DegreesToRadians(Angle);
	return Horizontal * FMath::Cos(Radians) + UnitUp * FMath::Sin(Radians);
}

FVector UGSTN_HitMathUtils::ApplyDI(const FVector& Launch, const FVector& StickOnPlane, const FVector& Up, const FVector& YawSourceDirection, const FGSTN_DIParams& Params)
{
	const FVector UnitUp = Up.GetSafeNormal();
	const double Speed = Launch.Size();
	const FVector Stick = ProjectOnPlane(StickOnPlane, UnitUp);
	const double StickLength = Stick.Size();
	if (UnitUp.IsZero() || Speed < UE_DOUBLE_SMALL_NUMBER || StickLength < UE_DOUBLE_SMALL_NUMBER)
	{
		return Launch;
	}

	// 1. Elevation dot and horizontal direction; a launch along Up has no horizontal part, the hit's yaw source supplies one
	const FVector Direction = Launch / Speed;
	const double Elevation = FVector::DotProduct(Direction, UnitUp);
	FVector Horizontal = (Direction - UnitUp * Elevation);
	const double HorizontalLength = Horizontal.Size();
	Horizontal = HorizontalLength < 1.0e-6 ? FirstHorizontal(UnitUp, YawSourceDirection, FVector::ZeroVector, FVector::ZeroVector) : Horizontal / HorizontalLength;
	const FVector Lateral = FVector::CrossProduct(UnitUp, Horizontal);

	// 2. Stick strength (linear, scale 1 in M4: M6 adds the curve and the victim scales) and its two components
	const double Strength = FMath::Min(StickLength, 1.0);
	const FVector StickDirection = Stick / StickLength;
	const double Along = Strength * FVector::DotProduct(StickDirection, Horizontal);
	const double Across = Strength * FVector::DotProduct(StickDirection, Lateral);

	// 3. Yaw about Up (right handed: a positive lateral component turns the launch towards Up x Horizontal)
	const FVector NewHorizontal = Horizontal.RotateAngleAxis(Params.MaxDIYawDegrees * Across, UnitUp);

	// 4. Side, 5. elevation in dot space
	const double Side = Elevation > Params.UpwardThresholdDot ? 1.0 : -1.0;
	const double Delta = Params.MaxDIElevationDot * (-Along);
	double NewElevation = Elevation + Side * Delta;

	// 6. DI out never crosses the horizontal
	if (Delta < 0.0)
	{
		NewElevation = Side > 0.0 ? FMath::Max(NewElevation, FMath::Min(Elevation, 0.0)) : FMath::Min(NewElevation, FMath::Max(Elevation, 0.0));
	}

	// 7. Keep a horizontal direction for the yaw
	NewElevation = FMath::Clamp(NewElevation, -static_cast<double>(Params.MaxElevationDot), static_cast<double>(Params.MaxElevationDot));

	// 8. Rebuild at the original speed
	return (NewHorizontal * FMath::Sqrt(1.0 - NewElevation * NewElevation) + UnitUp * NewElevation) * Speed;
}
