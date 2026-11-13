// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformScriptedPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformScriptedPath)

double FGSTN_PlatformScriptedPath::TriangleWave(double TimeSeconds, double Slope, double Amplitude)
{
	if (Slope <= 0.0 || Amplitude <= 0.0)
	{
		return 0.0;
	}
	// One period is 4 * Amplitude / Slope: up to +A, down to -A, back to 0
	const double Period = 4.0 * Amplitude / Slope;
	const double Phase = FMath::Fmod(TimeSeconds, Period) / Period;
	const double P = Phase < 0.0 ? Phase + 1.0 : Phase;
	if (P < 0.25)
	{
		return P * 4.0 * Amplitude;
	}
	if (P < 0.75)
	{
		return (0.5 - P) * 4.0 * Amplitude;
	}
	return (P - 1.0) * 4.0 * Amplitude;
}

FTransform FGSTN_PlatformScriptedPath::Evaluate(const FTransform& Origin, double TimeSeconds) const
{
	const FQuat OriginRotation = Origin.GetRotation();
	const FVector Offset = OriginRotation.RotateVector(Axis.GetSafeNormal()) * TriangleWave(TimeSeconds, Speed, HalfLength);
	const double Yaw = TriangleWave(TimeSeconds, YawRate, YawRange);
	const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(Yaw));
	return FTransform(Turn * OriginRotation, Origin.GetLocation() + Offset);
}
