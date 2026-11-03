// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_DashTypes.h"

FVector FGSTN_DashMath::HorizontalForward(const FVector& Up, const FVector& Facing, const FVector& Fallback)
{
	const FVector Candidates[2] = { Facing, Fallback };
	for (const FVector& Candidate : Candidates)
	{
		const FVector Flat = Candidate - Up * Candidate.Dot(Up);
		if (Flat.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			return Flat.GetSafeNormal();
		}
	}
	return FVector::ZeroVector;
}

FVector FGSTN_DashMath::ImpulseVelocity(const FVector& Up, const FVector& Forward, const FGSTN_DashMoveData& Data)
{
	return Forward * Data.ForwardSpeed + Up * Data.UpSpeed;
}

FVector FGSTN_DashMath::ResolveVelocity(const FVector& Prior, const FVector& Impulse, EGSTN_MoveMixMode Mix, EGSTN_MomentumMode Momentum)
{
	if (Momentum == EGSTN_MomentumMode::Discard || Mix == EGSTN_MoveMixMode::Override)
	{
		return Impulse;
	}
	return Prior + Impulse;
}

FGSTN_ArcPrediction FGSTN_DashMath::PredictArc(const FGSTN_DashMoveData& Data, float GravityAccel, float TickRate)
{
	FGSTN_ArcPrediction Arc;
	if (Data.UpSpeed <= 0.f || GravityAccel <= UE_KINDA_SMALL_NUMBER)
	{
		return Arc;
	}
	Arc.AirTimeSeconds = 2.f * Data.UpSpeed / GravityAccel;
	Arc.ApexHeight = Data.UpSpeed * Data.UpSpeed / (2.f * GravityAccel);
	Arc.Range = Data.ForwardSpeed * Arc.AirTimeSeconds;
	Arc.AirFrames = FMath::CeilToInt32(Arc.AirTimeSeconds * FMath::Max(TickRate, 1.f));
	return Arc;
}
