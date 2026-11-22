// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_GroundContactSolver.h"

double FGSTN_GroundContactSolver::RequiredLift(const FGSTN_GroundPlane& Plane, TConstArrayView<FVector> Points, TConstArrayView<double> Radii,
	double MaxLift)
{
	double Lift = 0.0;
	const int32 Num = FMath::Min(Points.Num(), Radii.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		Lift = FMath::Max(Lift, Penetration(Plane, Points[i], Radii[i]));
	}
	return FMath::Clamp(Lift, 0.0, FMath::Max(0.0, MaxLift));
}

double FGSTN_GroundContactSolver::SmoothLift(double Current, double Required, double DeltaTime, double HalfLife)
{
	if (Required >= Current || HalfLife <= 0.0 || DeltaTime <= 0.0)
	{
		return Required;
	}
	return Required + (Current - Required) * FMath::Pow(0.5, DeltaTime / HalfLife);
}

float FGSTN_GroundContactSolver::BlendWeight(float Weight, float Target, float DeltaTime, float BlendIn, float BlendOut)
{
	if (Target > Weight)
	{
		return BlendIn <= 0.f ? Target : FMath::Min(Target, Weight + DeltaTime / BlendIn);
	}
	return BlendOut <= 0.f ? Target : FMath::Max(Target, Weight - DeltaTime / BlendOut);
}

bool FGSTN_GroundContactSolver::LimbTarget(const FGSTN_GroundPlane& Plane, const FVector& Original, const FVector& Lifted, double Radius,
	double ContactBand, bool bKeepContact, double MaxCorrection, FVector& OutTarget)
{
	FVector Target;
	if (bKeepContact && Plane.Height(Original) < Radius + ContactBand)
	{
		// It was on the floor: it stays where the animation put it, only raised out of the floor
		Target = Original + Plane.Normal * Penetration(Plane, Original, Radius);
	}
	else
	{
		const double Sink = Penetration(Plane, Lifted, Radius);
		if (Sink <= UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		Target = Lifted + Plane.Normal * Sink;
	}
	const FVector Move = (Target - Lifted).GetClampedToMaxSize(FMath::Max(0.0, MaxCorrection));
	if (Move.SizeSquared() <= FMath::Square(UE_KINDA_SMALL_NUMBER))
	{
		return false;
	}
	OutTarget = Lifted + Move;
	return true;
}
