// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_Curve1D.generated.h"

/** One point of a FGSTN_Curve1D, in fixed point: X in 1/4096 steps of [0, 1], Y in Q2.14 (16384 is 1.0). */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_Curve1DPoint
{
	GENERATED_BODY()

	/** Input in 1/4096 steps (4096 is 1.0), for example the stick magnitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve", meta = (ClampMin = "0", ClampMax = "4096"))
	int32 X = 0;

	/** Output in Q2.14 (16384 is 1.0, so a factor up to 4 fits) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve", meta = (ClampMin = "0", ClampMax = "65535"))
	int32 Y = 16384;
};

/**
 * A piecewise-linear curve of one variable made for the simulation: the points are integers, the input is quantised to 1/4096
 * and the interpolation is integer arithmetic (round half up), so every machine reads the same number and no engine curve asset
 * (a float asset with its own evaluation) is involved. Used by the DI and SDI strength curves.
 *
 * Outside the first and last point the curve holds the end values. A curve with no point is not authored: it is the identity
 * (evaluates to 1.0), so a rules asset nobody tuned changes nothing. X must strictly increase from point to point (validation).
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_Curve1D
{
	GENERATED_BODY()

	static constexpr int32 XOne = 4096;
	static constexpr int32 YOne = 1 << 14;
	static constexpr int32 YMax = 65535;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curve")
	TArray<FGSTN_Curve1DPoint> Points;

	bool IsAuthored() const { return Points.Num() > 0; }

	/** Appends a point from real values, quantising and clamping them to the representable range */
	void AddPoint(double X, double Y);

	/** A straight line from the value at 0 to the value at 1 */
	static FGSTN_Curve1D MakeLinear(double AtZero, double AtOne);

	/** The quantised X for a real input: clamped to [0, 1], rounded half up to 1/4096 */
	static int32 QuantiseX(double X);

	/** The fixed-point value (YOne = 1.0) at X: integer interpolation after the quantisation of X. YOne when not authored */
	int32 EvaluateFixed(double X) const;

	/** EvaluateFixed as a double; always a multiple of 1/16384 */
	double Evaluate(double X) const { return static_cast<double>(EvaluateFixed(X)) / static_cast<double>(YOne); }

	/** True when authored and the outputs never decrease as the input grows */
	bool IsMonotonicNonDecreasing() const;

	/** Appends one message per problem: X out of range or not strictly increasing, Y out of range */
	void Validate(const FString& Label, TArray<FString>& OutErrors) const;
};
