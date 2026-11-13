// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_ReactionTypes.h"

bool FGSTN_BakedCurve::IsFlat() const
{
	if (!IsAuthored())
	{
		return false;
	}
	for (int32 Index = 1; Index < SampleCount; ++Index)
	{
		if (Samples[Index] != Samples[0])
		{
			return false;
		}
	}
	return true;
}

void FGSTN_BakedCurve::Bake(TFunctionRef<double(double)> Function)
{
	Samples.SetNumUninitialized(SampleCount);
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double Value = Function(static_cast<double>(Index) / static_cast<double>(SampleCount - 1));
		Samples[Index] = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(Value * One), 0, 65535));
	}
}

void FGSTN_BakedCurve::BakeConstant(double Value)
{
	Bake([Value](double) { return Value; });
}

void FGSTN_BakedCurve::BakeLinear(double AtZero, double AtOne)
{
	Bake([AtZero, AtOne](double T) { return AtZero + (AtOne - AtZero) * T; });
}

int32 FGSTN_BakedCurve::EvaluateFixed(double T) const
{
	if (!IsAuthored())
	{
		return One;
	}

	// !(T >= 0) also catches NaN. The input is quantised first so the rest is integer arithmetic
	const double Clamped = !(T >= 0.0) ? 0.0 : FMath::Min(T, 1.0);
	const int32 Quantised = FMath::Clamp(FMath::RoundToInt(Clamped * 4096.0), 0, 4096);
	const int32 Position = Quantised * (SampleCount - 1);
	const int32 Index = Position >> 12;
	const int32 Fraction = Position & 4095;
	if (Index >= SampleCount - 1)
	{
		return Samples[SampleCount - 1];
	}
	const int32 From = Samples[Index];
	const int32 To = Samples[Index + 1];
	return From + static_cast<int32>((static_cast<int64>(To - From) * Fraction) / 4096);
}

FGSTN_VictimReactionState FGSTN_VictimReactionState::Make(double DamagePercent, double WeightScale, uint8 InFlags, const FGSTN_StaggerThreshold& Threshold, uint8 InExtMask)
{
	FGSTN_VictimReactionState State;
	State.DamagePercentQ = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(DamagePercent * 10.0), 0, 65535));
	State.WeightScaleQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(WeightScale * 64.0), 0, 255));
	State.Flags = InFlags;
	State.ExtMask = InExtMask;
	if (Threshold.bInfinite)
	{
		State.EffectiveThresholdQ.bInfinite = true;
	}
	else
	{
		State.EffectiveThresholdQ.Value = Threshold.Value < 0.f ? -1.f : static_cast<float>(FMath::RoundToInt(Threshold.Value));
	}
	return State;
}
