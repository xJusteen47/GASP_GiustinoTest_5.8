// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsLimits.h"

struct FRichCurve;

/**
 * A curve over 0..1 baked into 64 integer entries (combat-parts.md §4.3). Authored as an ordinary float curve, baked once at load,
 * and read with integer arithmetic only, so every machine gets the identical number and the float curve evaluator is never on
 * the simulation path.
 *
 * Values are unsigned 16 bit fractions: 0 is 0.0 and 65535 is 1.0, so the curve describes a scale of at most one (the weight
 * scale of a battered enemy). A sample outside 0..1 is clamped when baking. The input is quantised to the same 16 bits and the
 * lookup interpolates linearly between the two neighbouring entries with 64 bit integer arithmetic.
 *
 * Not a reflected struct: it is plain data held by the asset that bakes it and rebuilt on load, never serialised or sent.
 *
 * The spec names this FGSTN_BakedCurve and the hit plan declares a type of that name in GSTNCombatCore for its health-weight
 * step; two types of one name could not be included together, so this one carries the plugin's name until the combat one exists.
 */
struct GSTNPARTSCORE_API FGSTN_PartsBakedCurve
{
	/** 1.0 as a quantised fraction */
	static constexpr uint32 One = 65535;

	/** Entry i is the curve at i / (CurveEntries - 1) */
	uint16 Table[GSTN_PartsLimits::CurveEntries];

	/** A flat curve at 1.0 */
	FGSTN_PartsBakedCurve();

	/** Flat at Value (clamped to 0..1) */
	static FGSTN_PartsBakedCurve MakeFlat(float Value);

	/** Linear from AtZero (progress 0) to AtOne (progress 1), both clamped to 0..1, interpolated in integers */
	static FGSTN_PartsBakedCurve MakeLinear(float AtZero, float AtOne);

	/** Samples a float curve at the 64 positions. The only float evaluation, done at load, never in the simulation */
	static FGSTN_PartsBakedCurve FromRichCurve(const FRichCurve& Curve);

	/** From explicit samples (fewer than 64 are padded with the last, extra ones are ignored; none = flat 1.0) */
	static FGSTN_PartsBakedCurve FromSamples(TConstArrayView<float> Samples);

	/** A value in 0..1 as a quantised fraction (clamped, rounded to nearest, NaN = 0) */
	static uint32 QuantiseUnit(float Value);

	/** Quantised fraction back to a float; exact division by One, so equal inputs give equal floats on every machine */
	static float DequantiseUnit(uint32 Quantised) { return static_cast<float>(Quantised) / static_cast<float>(One); }

	/** The curve at a quantised progress (0..One, larger values clamp): integers only, returns 0..One */
	uint32 SampleQuantised(uint32 ProgressQuantised) const;

	/** The curve at a progress in 0..1 as a float scale: quantises the progress, then SampleQuantised */
	float Sample(float Progress) const { return DequantiseUnit(SampleQuantised(QuantiseUnit(Progress))); }

	bool operator==(const FGSTN_PartsBakedCurve& Other) const;
	bool operator!=(const FGSTN_PartsBakedCurve& Other) const { return !(*this == Other); }
};
