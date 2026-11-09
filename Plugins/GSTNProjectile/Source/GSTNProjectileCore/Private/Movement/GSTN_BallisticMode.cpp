// Projectile, pure part (no GAS, no Network Prediction, no Mover).

#include "Movement/GSTN_BallisticMode.h"

#include "Hits/GSTN_HitShapeUtils.h"

FGSTN_BallisticSegment FGSTN_BallisticMode::MakeSegment(const FGSTN_ProjectileParams& Params, int32 StartFrame, double StepSeconds)
{
	FGSTN_BallisticSegment Segment;
	Segment.StartFrame = StartFrame;
	Segment.Start = Params.Origin;
	Segment.Velocity = Params.Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector) * static_cast<double>(Params.Speed);
	Segment.GravityAccel = Params.GravityAccel;
	Segment.StepSeconds = StepSeconds;
	return Segment;
}

FVector FGSTN_BallisticMode::PositionAt(const FGSTN_BallisticSegment& Segment, int32 Frame)
{
	const double T = static_cast<double>(FMath::Max(Frame - Segment.StartFrame, 0)) * Segment.StepSeconds;
	return Segment.Start + Segment.Velocity * T + Segment.GravityAccel * (0.5 * T * T);
}

FVector FGSTN_BallisticMode::VelocityAt(const FGSTN_BallisticSegment& Segment, int32 Frame)
{
	const double T = static_cast<double>(FMath::Max(Frame - Segment.StartFrame, 0)) * Segment.StepSeconds;
	return Segment.Velocity + Segment.GravityAccel * T;
}

FTransform FGSTN_BallisticMode::CarrierAt(const FGSTN_BallisticSegment& Segment, int32 Frame)
{
	const FVector Launch = Segment.Velocity.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Forward = VelocityAt(Segment, Frame).GetSafeNormal(UE_SMALL_NUMBER, Launch);
	return FTransform(FRotationMatrix::MakeFromX(Forward).ToQuat(), PositionAt(Segment, Frame));
}

bool FGSTN_BallisticMode::SweepStep(const FGSTN_BallisticSegment& Segment, const FGSTN_HitShape& Shape, int32 Frame, const FGSTN_HitShape& Target, const FTransform& TargetWorld)
{
	return UGSTN_HitShapeUtils::SweptOverlap(Shape, CarrierAt(Segment, Frame - 1), CarrierAt(Segment, Frame), Target, TargetWorld);
}

int32 FGSTN_BallisticMode::FindFirstContactFrame(const FGSTN_BallisticSegment& Segment, const FGSTN_HitShape& Shape, const FGSTN_HitShape& Target, const FTransform& TargetWorld, int32 LastFrame)
{
	for (int32 Frame = Segment.StartFrame + 1; Frame <= LastFrame; ++Frame)
	{
		if (SweepStep(Segment, Shape, Frame, Target, TargetWorld))
		{
			return Frame;
		}
	}
	return INDEX_NONE;
}
