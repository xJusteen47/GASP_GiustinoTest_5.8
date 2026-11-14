// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_MomentumSlide.h"

const FGSTN_MomentumSlideResponse* FGSTN_MomentumSlideMath::PickResponse(const FGSTN_MomentumSlide& Slide, const FVector& FlatVelocity, const FVector& FlatForward, bool& bOutAgainst)
{
	bOutAgainst = false;
	const double Speed = FlatVelocity.Size();
	if (!Slide.bEnabled || Speed <= UE_KINDA_SMALL_NUMBER || Speed < Slide.MinSpeed)
	{
		return nullptr;
	}
	// A facing that does not lie in the ground plane (should not happen on the ground) counts as along the momentum
	const double Dot = FlatForward.IsNearlyZero() ? 1.0 : (FlatVelocity / Speed).Dot(FlatForward.GetSafeNormal());
	bOutAgainst = Dot < Slide.OppositeDotThreshold;
	const FGSTN_MomentumSlideResponse& Response = bOutAgainst ? Slide.Against : Slide.Along;
	return Response.Mode == EGSTN_MomentumSlideMode::Slide ? &Response : nullptr;
}

float FGSTN_MomentumSlideMath::StartSpeed(const FGSTN_MomentumSlideResponse& Response, float RunningSpeed)
{
	const float Scaled = FMath::Max(RunningSpeed, 0.f) * FMath::Max(Response.SpeedScale, 0.f);
	return Response.MaxStartSpeed > 0.f ? FMath::Min(Scaled, Response.MaxStartSpeed) : Scaled;
}

FGSTN_MomentumSlideBraking FGSTN_MomentumSlideMath::ResolveBraking(const FGSTN_MomentumSlideResponse& Response, float CharacterDeceleration, float CharacterFriction)
{
	FGSTN_MomentumSlideBraking Braking;
	if (Response.Friction == EGSTN_MomentumSlideFriction::Character)
	{
		const float Scale = FMath::Max(Response.FrictionScale, 0.f);
		Braking.Deceleration = FMath::Max(CharacterDeceleration, 0.f) * Scale;
		Braking.Friction = FMath::Max(CharacterFriction, 0.f) * Scale;
	}
	else
	{
		Braking.Deceleration = FMath::Max(Response.Deceleration, 0.f);
	}
	return Braking;
}

float FGSTN_MomentumSlideMath::StepSpeed(float Speed, const FGSTN_MomentumSlideBraking& Braking, float DeltaSeconds)
{
	const float Loss = (Braking.Deceleration + Braking.Friction * Speed) * FMath::Max(DeltaSeconds, 0.f);
	return FMath::Max(Speed - Loss, 0.f);
}

float FGSTN_MomentumSlideMath::PredictDistance(const FGSTN_MomentumSlideResponse& Response, float RunningSpeed, const FGSTN_MomentumSlideBraking& Braking, float TickRate, int32 MaxSteps)
{
	if (Response.Mode != EGSTN_MomentumSlideMode::Slide || TickRate <= 0.f)
	{
		return 0.f;
	}
	// The same order as the move: a frame moves at the speed it starts with, then the speed steps down
	const float Dt = 1.f / TickRate;
	const int32 Steps = Response.MaxFrames > 0 ? FMath::Min(Response.MaxFrames, MaxSteps) : MaxSteps;
	float Speed = StartSpeed(Response, RunningSpeed);
	float Distance = 0.f;
	for (int32 Step = 0; Step < Steps && Speed > 0.f; ++Step)
	{
		Distance += Speed * Dt;
		Speed = StepSpeed(Speed, Braking, Dt);
	}
	return Distance;
}
