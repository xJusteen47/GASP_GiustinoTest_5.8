// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PartsBakedCurve.h"

#include "Curves/RichCurve.h"

namespace
{
	constexpr int32 CurveLastIndex = GSTN_PartsLimits::CurveEntries - 1;
}

FGSTN_PartsBakedCurve::FGSTN_PartsBakedCurve()
{
	for (uint16& Entry : Table)
	{
		Entry = static_cast<uint16>(One);
	}
}

uint32 FGSTN_PartsBakedCurve::QuantiseUnit(float Value)
{
	if (!(Value > 0.f))
	{
		return 0; // also NaN
	}
	if (Value >= 1.f)
	{
		return One;
	}
	return static_cast<uint32>(FMath::FloorToInt(Value * static_cast<float>(One) + 0.5f));
}

FGSTN_PartsBakedCurve FGSTN_PartsBakedCurve::MakeFlat(float Value)
{
	FGSTN_PartsBakedCurve Curve;
	const uint16 Quantised = static_cast<uint16>(QuantiseUnit(Value));
	for (uint16& Entry : Curve.Table)
	{
		Entry = Quantised;
	}
	return Curve;
}

FGSTN_PartsBakedCurve FGSTN_PartsBakedCurve::MakeLinear(float AtZero, float AtOne)
{
	FGSTN_PartsBakedCurve Curve;
	const int64 From = QuantiseUnit(AtZero);
	const int64 To = QuantiseUnit(AtOne);
	for (int32 Index = 0; Index <= CurveLastIndex; ++Index)
	{
		// Rounded to nearest in integers, away from zero on a tie, so the table never depends on float rounding
		const int64 Numerator = (To - From) * Index;
		const int64 Offset = Numerator >= 0 ? (Numerator + CurveLastIndex / 2) / CurveLastIndex : -((-Numerator + CurveLastIndex / 2) / CurveLastIndex);
		Curve.Table[Index] = static_cast<uint16>(From + Offset);
	}
	return Curve;
}

FGSTN_PartsBakedCurve FGSTN_PartsBakedCurve::FromRichCurve(const FRichCurve& Curve)
{
	FGSTN_PartsBakedCurve Baked;
	for (int32 Index = 0; Index <= CurveLastIndex; ++Index)
	{
		Baked.Table[Index] = static_cast<uint16>(QuantiseUnit(Curve.Eval(static_cast<float>(Index) / static_cast<float>(CurveLastIndex))));
	}
	return Baked;
}

FGSTN_PartsBakedCurve FGSTN_PartsBakedCurve::FromSamples(TConstArrayView<float> Samples)
{
	FGSTN_PartsBakedCurve Baked;
	if (Samples.IsEmpty())
	{
		return Baked;
	}
	for (int32 Index = 0; Index <= CurveLastIndex; ++Index)
	{
		Baked.Table[Index] = static_cast<uint16>(QuantiseUnit(Samples[FMath::Min(Index, Samples.Num() - 1)]));
	}
	return Baked;
}

uint32 FGSTN_PartsBakedCurve::SampleQuantised(uint32 ProgressQuantised) const
{
	const uint32 Progress = FMath::Min(ProgressQuantised, One);
	// Position in 64-entry space scaled by One: at most 63 * 65535, which fits comfortably
	const uint32 Position = Progress * static_cast<uint32>(CurveLastIndex);
	const uint32 Index = Position / One;
	if (Index >= static_cast<uint32>(CurveLastIndex))
	{
		return Table[CurveLastIndex];
	}
	const int64 Fraction = Position % One;
	const int64 Low = Table[Index];
	const int64 Delta = static_cast<int64>(Table[Index + 1]) - Low;
	// C++ integer division truncates toward zero, which is the defined and identical result on every machine
	return static_cast<uint32>(Low + (Delta * Fraction) / static_cast<int64>(One));
}

bool FGSTN_PartsBakedCurve::operator==(const FGSTN_PartsBakedCurve& Other) const
{
	return FMemory::Memcmp(Table, Other.Table, sizeof(Table)) == 0;
}
