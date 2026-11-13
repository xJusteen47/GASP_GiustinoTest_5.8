// Projectile, pure part.

#include "Movement/GSTN_ShotAim.h"

void FGSTN_ShotAim::MakeHitscan(FGSTN_ProjectileParams& InOut, const FGSTN_HitscanSpec& Spec)
{
	const double Range = FMath::Max(1.0, static_cast<double>(Spec.RangeCm));
	const double Radius = FMath::Max(0.1, static_cast<double>(Spec.RadiusCm));
	InOut.Speed = HitscanCarrierSpeed;
	InOut.GravityAccel = FVector::ZeroVector;
	InOut.LifetimeFrames = 1;
	InOut.bHitNearestFirst = true;
	InOut.bRadialPush = false;

	// The capsule axis is local Z: pitched -90 it lies along the carrier's X, centred halfway down the line. The half height includes the
	// hemispheres, so the line covers [-Radius, Range + Radius] from the muzzle
	InOut.Shape.Type = EGSTN_ShapeType::Capsule;
	InOut.Shape.Extent = FVector(Radius, Radius, FMath::Max(Radius, Range * 0.5 + Radius));
	InOut.Shape.Local = FTransform(FRotator(-90.0, 0.0, 0.0), FVector(Range * 0.5, 0.0, 0.0));
}

FVector FGSTN_ShotAim::ReticleDirection(bool bReticleOn, const FRotator& ReticleAim, const FVector& FlatDirection, float MinPitchDeg, float MaxPitchDeg, float LaunchPitchDeg)
{
	const FVector Flat = FlatDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	if (bReticleOn)
	{
		FRotator Aim = ReticleAim.IsNearlyZero() ? Flat.Rotation() : ReticleAim;
		const double Low = FMath::Min(MinPitchDeg, MaxPitchDeg);
		const double High = FMath::Max(MinPitchDeg, MaxPitchDeg);
		Aim.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch), FMath::Max(Low, -89.0), FMath::Min(High, 89.0));
		Aim.Roll = 0.0;
		return Aim.Vector();
	}
	FRotator Launch = Flat.Rotation();
	Launch.Pitch = FMath::Clamp(Launch.Pitch + LaunchPitchDeg, -89.0, 89.0);
	return Launch.Vector();
}
