// Pure combat logic: the analog walk start and the air turn.

#include "Movement/GSTN_AnalogWalkTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnalogWalkTypes)

float FGSTN_AnalogWalkLogic::StickToIntent(const FGSTN_AnalogWalkTuning& Tuning, float StickSize)
{
	const float Size = FMath::Clamp(StickSize, 0.f, 1.f);
	const float Dead = FMath::Clamp(Tuning.InnerDeadzone, 0.f, 0.9f);
	if (Size <= Dead)
	{
		return 0.f;
	}
	const float Full = FMath::Max(Tuning.FullTiltAt, Dead + KINDA_SMALL_NUMBER);
	const float Tilt = FMath::Clamp((Size - Dead) / (Full - Dead), 0.f, 1.f);
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
