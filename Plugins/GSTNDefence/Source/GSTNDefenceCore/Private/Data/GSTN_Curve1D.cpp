// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_Curve1D.h"

namespace
{
	/** Integer division rounding half up, for a non-negative numerator and a positive divisor */
	int64 DivRoundHalfUp(int64 Numerator, int64 Divisor)
	{
		return (2 * Numerator + Divisor) / (2 * Divisor);
	}
}

int32 FGSTN_Curve1D::QuantiseX(double X)
{
	if (!(X > 0.0))
	{
		return 0;
	}
	if (X >= 1.0)
	{
		return XOne;
	}
	return static_cast<int32>(FMath::FloorToInt64(X * XOne + 0.5));
}

void FGSTN_Curve1D::AddPoint(double X, double Y)
{
	FGSTN_Curve1DPoint Point;
	Point.X = QuantiseX(X);
	Point.Y = FMath::Clamp(static_cast<int32>(FMath::FloorToInt64(Y * YOne + 0.5)), 0, YMax);
	Points.Add(Point);
}

FGSTN_Curve1D FGSTN_Curve1D::MakeLinear(double AtZero, double AtOne)
{
	FGSTN_Curve1D Curve;
	Curve.AddPoint(0.0, AtZero);
	Curve.AddPoint(1.0, AtOne);
	return Curve;
}

int32 FGSTN_Curve1D::EvaluateFixed(double X) const
{
	if (Points.IsEmpty())
	{
		return YOne;
	}

	const int32 Xq = QuantiseX(X);
	if (Xq <= Points[0].X)
	{
		return Points[0].Y;
	}
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const FGSTN_Curve1DPoint& High = Points[Index];
		if (Xq <= High.X)
		{
			const FGSTN_Curve1DPoint& Low = Points[Index - 1];
			const int64 Span = static_cast<int64>(High.X) - Low.X;
			if (Span <= 0)
			{
				return High.Y;
			}
			const int64 Delta = static_cast<int64>(High.Y) - Low.Y;
			const int64 Along = static_cast<int64>(Xq) - Low.X;
			// Interpolate the magnitude and restore the sign, so a falling segment rounds the same way as a rising one
			const int64 Step = DivRoundHalfUp(FMath::Abs(Delta) * Along, Span);
			return static_cast<int32>(Low.Y + (Delta < 0 ? -Step : Step));
		}
	}
	return Points.Last().Y;
}

bool FGSTN_Curve1D::IsMonotonicNonDecreasing() const
{
	if (Points.IsEmpty())
	{
		return false;
	}
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		if (Points[Index].Y < Points[Index - 1].Y)
		{
			return false;
		}
	}
	return true;
}

void FGSTN_Curve1D::Validate(const FString& Label, TArray<FString>& OutErrors) const
{
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FGSTN_Curve1DPoint& Point = Points[Index];
		if (Point.X < 0 || Point.X > XOne)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: point %d has X %d outside 0..%d."), *Label, Index, Point.X, XOne));
		}
		if (Point.Y < 0 || Point.Y > YMax)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: point %d has Y %d outside 0..%d."), *Label, Index, Point.Y, YMax));
		}
		if (Index > 0 && Point.X <= Points[Index - 1].X)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: point %d does not have a larger X than the point before it; X must strictly increase."), *Label, Index));
		}
	}
}
