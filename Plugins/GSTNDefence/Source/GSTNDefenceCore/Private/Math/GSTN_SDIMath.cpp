// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "Math/GSTN_SDIMath.h"

#include "Math/GSTN_DIRulesMath.h"

FGSTN_SDIStep UGSTN_SDIMath::NextDisplacement(const FGSTN_SDIRules& Rules, const FVector& StickOnPlane, double AccumulatedDistanceCm, bool bArmored, bool bStaggerInPlace, bool bThrow, const FGameplayTagContainer& VictimTags, int32 HitstopFrames, double FrameSeconds)
{
	FGSTN_SDIStep Step;

	if (bThrow || (bArmored && !Rules.bSDIAllowedWhenArmored) || (bStaggerInPlace && !Rules.bSDIInStaggerInPlace))
	{
		Step.bBlocked = true;
		return Step;
	}

	const double StickSize = StickOnPlane.Size();
	if (StickSize <= 0.0 || StickSize < static_cast<double>(Rules.MinZone) || FrameSeconds <= 0.0)
	{
		return Step;
	}

	const double Remaining = static_cast<double>(Rules.MaxDistanceCmPerHit) - AccumulatedDistanceCm;
	// A hair of floating point residue after the last frame must not start another one
	constexpr double Epsilon = 1e-9;
	if (Remaining <= Epsilon)
	{
		return Step;
	}

	const double Magnitude = FMath::Min(1.0, StickSize);
	double Speed = Magnitude * Rules.StrengthCurve.Evaluate(Magnitude) * static_cast<double>(Rules.DriftSpeedCmPerSec)
		* UGSTN_DIRulesMath::ScaleByVictimTag(Rules.ScaleByVictimTag, VictimTags);
	if (Rules.bScaleByHitstopFrames && HitstopFrames > 0)
	{
		Speed = FMath::Min(Speed, static_cast<double>(Rules.MaxDistanceCmPerHit) / (static_cast<double>(HitstopFrames) * FrameSeconds));
	}
	if (Speed <= 0.0)
	{
		return Step;
	}

	double Distance = Speed * FrameSeconds;
	if (Distance > Remaining)
	{
		// The frame that reaches the maximum drifts only what is left
		Speed = Remaining / FrameSeconds;
		Distance = Remaining;
	}

	Step.DriftVelocity = StickOnPlane / StickSize * Speed;
	Step.DistanceAdded = Distance;
	return Step;
}
