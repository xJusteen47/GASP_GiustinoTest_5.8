// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_CoveredLanding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CoveredLanding)

FGSTN_CoveredLandingOutput FGSTN_CoveredLandingLogic::Step(const FGSTN_CoveredLandingSettings& Settings, float DeltaSeconds, float UpSpeed,
	float FloorSpeed, bool bMontageDriving, float MontageWeight)
{
	FGSTN_CoveredLandingOutput Out;
	if (!Settings.bEnabled)
	{
		Reset();
		return Out;
	}

	// Touchdown: the first frame after a fall whose downward speed dropped under StoppedSpeed. A capsule growing on that frame (the body capsule's
	// grounded restore lifts the actor) reads as an upward spike: still a touchdown
	const bool bTouchdown = bFalling && UpSpeed > -Settings.StoppedSpeed;
	if (UpSpeed <= -Settings.FallSpeed)
	{
		bFalling = true;
	}
	else if (bTouchdown)
	{
		bFalling = false;
	}

	if (bTouchdown && bMontageDriving)
	{
		// Covered: the base layer starts its landing reaction under the montage
		WindowTime = 0.f;
		bCoverStopped = false;
		bCaptured = false;
		EarlyFade = -1.f;
	}
	else if (WindowTime >= 0.f)
	{
		WindowTime += DeltaSeconds;
	}
	if (WindowTime < 0.f)
	{
		return Out;
	}

	if (bMontageDriving)
	{
		if (bCoverStopped)
		{
			// Another montage took over after the covering one: it owns the pose, the window is over
			WindowTime = -1.f;
			return Out;
		}
		if (MontageWeight >= 0.99f)
		{
			Out.bCapture = true;
			bCaptured = true;
		}
		return Out;
	}
	bCoverStopped = true;

	const float End = Settings.HoldUntil + Settings.BlendOut;
	if (!bCaptured || WindowTime >= End)
	{
		WindowTime = -1.f;
		return Out;
	}
	// Walking away or leaving the ground: the live pose comes back over BlendOut from wherever the hold was
	if (EarlyFade < 0.f && (FloorSpeed > Settings.MoveSpeed || UpSpeed <= -Settings.FallSpeed || UpSpeed >= Settings.FallSpeed))
	{
		EarlyFade = 0.f;
	}
	float Alpha = WindowTime <= Settings.HoldUntil ? 1.f : 1.f - (WindowTime - Settings.HoldUntil) / FMath::Max(Settings.BlendOut, 0.01f);
	if (EarlyFade >= 0.f)
	{
		EarlyFade += DeltaSeconds;
		Alpha = FMath::Min(Alpha, 1.f - EarlyFade / FMath::Max(Settings.BlendOut, 0.01f));
	}
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (Alpha <= 0.f)
	{
		WindowTime = -1.f;
		return Out;
	}
	// Smooth both ends of the blend
	Out.HoldAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
	return Out;
}
