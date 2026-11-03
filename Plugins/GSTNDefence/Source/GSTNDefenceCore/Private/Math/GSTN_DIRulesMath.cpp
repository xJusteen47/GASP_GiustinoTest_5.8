// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "Math/GSTN_DIRulesMath.h"

#include "Hits/GSTN_HitMathUtils.h"

double UGSTN_DIRulesMath::StrengthCurve(const FGSTN_DIRules& Rules, double StickMagnitude)
{
	return Rules.StrengthCurve.Evaluate(StickMagnitude);
}

double UGSTN_DIRulesMath::ElevationDeltaByElevation(const FGSTN_DIRules& Rules, double AbsElevationDot)
{
	return Rules.ElevationDeltaByElevation.Evaluate(AbsElevationDot);
}

double UGSTN_DIRulesMath::ScaleByVictimTag(const TMap<FGameplayTag, float>& Scales, const FGameplayTagContainer& VictimTags)
{
	double Scale = 1.0;
	for (const TPair<FGameplayTag, float>& Pair : Scales)
	{
		if (Pair.Key.IsValid() && VictimTags.HasTag(Pair.Key))
		{
			Scale *= static_cast<double>(Pair.Value);
		}
	}
	return Scale;
}

FVector UGSTN_DIRulesMath::ApplyDI(const FVector& Launch, const FVector& StickOnPlane, const FVector& Up, const FVector& YawSourceDirection, double KnockbackMagnitude, const FGameplayTagContainer& VictimTags, const FGSTN_DIRules& Rules, bool bHitDisablesDI)
{
	if (bHitDisablesDI || KnockbackMagnitude < static_cast<double>(Rules.MinKnockbackForDI))
	{
		return Launch;
	}

	const double StickSize = StickOnPlane.Size();
	if (StickSize <= 0.0)
	{
		return Launch;
	}

	const double Magnitude = FMath::Min(1.0, StickSize);
	const double Strength = FMath::Min(1.0, Magnitude * StrengthCurve(Rules, Magnitude) * ScaleByVictimTag(Rules.ScaleByVictimTag, VictimTags));
	if (Strength <= 0.0)
	{
		return Launch;
	}

	FGSTN_DIParams Params = Rules.Params;
	const double AbsElevation = FMath::Abs(FVector::DotProduct(Launch.GetSafeNormal(), Up.GetSafeNormal()));
	Params.MaxDIElevationDot = static_cast<float>(FMath::Clamp(static_cast<double>(Params.MaxDIElevationDot) * ElevationDeltaByElevation(Rules, AbsElevation), 0.0, 1.0));

	return UGSTN_HitMathUtils::ApplyDI(Launch, StickOnPlane / StickSize * Strength, Up, YawSourceDirection, Params);
}
