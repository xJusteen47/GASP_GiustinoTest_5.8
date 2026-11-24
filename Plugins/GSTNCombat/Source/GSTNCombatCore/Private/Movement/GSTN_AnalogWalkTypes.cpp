// Pure combat logic: the analog walk start and the air turn.

#include "Movement/GSTN_AnalogWalkTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnalogWalkTypes)

float FGSTN_AnalogWalkLogic::StickToIntent(const FGSTN_AnalogWalkTuning& Tuning, float StickSize)
{
	const float Size = FMath::Clamp(StickSize, 0.f, 1.f);
	// No second dead zone (owner 2026-11-24, 1A): the player's dead zone (UGSTN_InputOptionsSubsystem) is the only one
	if (Size <= 0.f)
	{
		return 0.f;
	}
	const float Full = FMath::Max(Tuning.FullTiltAt, KINDA_SMALL_NUMBER);
	const float Tilt = FMath::Clamp(Size / Full, 0.f, 1.f);
	const float MinFraction = FMath::Clamp(Tuning.MinSpeedFraction, 0.f, 1.f);
	return MinFraction + (1.f - MinFraction) * FMath::Pow(Tilt, FMath::Max(Tuning.ResponseExponent, 0.25f));
}

float FGSTN_AnalogWalkLogic::StartBlend(const FGSTN_AnalogWalkTuning& Tuning, float GroundSpeed, float TopSpeed)
{
	if (TopSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	const float Fraction = GroundSpeed / TopSpeed;
	return 1.f - FMath::Clamp(Fraction / FMath::Max(Tuning.LowSpeedFraction, 0.01f), 0.f, 1.f);
}

float FGSTN_AnalogWalkLogic::StepAirYaw(const FGSTN_AnalogWalkTuning& Tuning, float CurrentYaw, float TargetYaw, float DeltaSeconds)
{
	const float Delta = FRotator::NormalizeAxis(TargetYaw - CurrentYaw);
	const float MaxStep = FMath::Max(Tuning.AirTurnRateDegPerSec, 0.f) * FMath::Max(DeltaSeconds, 0.f);
	return FRotator::NormalizeAxis(CurrentYaw + FMath::Clamp(Delta, -MaxStep, MaxStep));
}

float FGSTN_AnalogWalkLogic::AirTurnYawRate(float RateDegPerSec, float CurrentYaw, float TargetYaw, float DeltaSeconds)
{
	if (RateDegPerSec <= 0.f || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	// Same side rule as StepAirYaw (NormalizeAxis: an exact half turn goes positive), so the step the input asked for is the one taken
	const float Delta = FRotator::NormalizeAxis(TargetYaw - CurrentYaw);
	return FMath::Clamp(Delta / DeltaSeconds, -RateDegPerSec, RateDegPerSec);
}
