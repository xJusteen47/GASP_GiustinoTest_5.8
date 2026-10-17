// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_CombatInputSettings.h"

namespace
{
	/** Angle brought to [0, 360) */
	float NormalizeAngle(float AngleDeg)
	{
		float Result = FMath::Fmod(AngleDeg, 360.f);
		if (Result < 0.f)
		{
			Result += 360.f;
		}
		// Fmod of a tiny negative value plus 360 can round to exactly 360
		return Result >= 360.f ? 0.f : Result;
	}
}

FGSTN_StickSample FGSTN_DirectionClassifier::Classify(const FVector2D& RawStick, float CameraYawDeg, float FacingYawDeg,
	const FGSTN_ClassifierState& Prev, int32 Frame, const UGSTN_CombatInputSettings& Settings, FGSTN_ClassifierState& OutState)
{
	FGSTN_StickSample Sample;
	Sample.Zone = ClassifyZone(static_cast<float>(RawStick.Size()), Settings);

	OutState.Zone = Sample.Zone;
	OutState.LastNeutralFrame = Sample.Zone == EGSTN_StickZone::Neutral ? Frame : Prev.LastNeutralFrame;

	if (Sample.Zone == EGSTN_StickZone::Neutral)
	{
		return Sample;
	}

	const float StickAngle = GetStickAngle(RawStick);
	const float WorldHeading = StickAngle + CameraYawDeg;
	const float HalfAngle = Settings.DiagonalHalfAngle;

	Sample.StickDir = AngleToDirection(StickAngle, HalfAngle);
	Sample.FacingDir = AngleToDirection(WorldHeading - FacingYawDeg, HalfAngle);
	Sample.WorldDir = AngleToDirection(WorldHeading - Settings.WorldReferenceYaw, HalfAngle);
	Sample.WorldYaw = QuantizeYaw(WorldHeading);

	// A flick is the frame the stick reaches Tilt after leaving Neutral no more than FlickFrames ago.
	if (Sample.Zone == EGSTN_StickZone::Tilt && Prev.Zone != EGSTN_StickZone::Tilt && Prev.LastNeutralFrame != INDEX_NONE)
	{
		Sample.bFlick = (Frame - Prev.LastNeutralFrame) <= Settings.FlickFrames;
	}

	return Sample;
}

EGSTN_StickZone FGSTN_DirectionClassifier::ClassifyZone(float Magnitude, const UGSTN_CombatInputSettings& Settings)
{
	if (Magnitude < Settings.StickDeadZone)
	{
		return EGSTN_StickZone::Neutral;
	}
	return Magnitude < Settings.AimZone ? EGSTN_StickZone::Aim : EGSTN_StickZone::Tilt;
}

EGSTN_StickDirection FGSTN_DirectionClassifier::AngleToDirection(float AngleDeg, float DiagonalHalfAngle)
{
	static constexpr EGSTN_StickDirection Cardinals[4] = {
		EGSTN_StickDirection::Up, EGSTN_StickDirection::Right, EGSTN_StickDirection::Down, EGSTN_StickDirection::Left };
	static constexpr EGSTN_StickDirection Diagonals[4] = {
		EGSTN_StickDirection::UpRight, EGSTN_StickDirection::DownRight, EGSTN_StickDirection::DownLeft, EGSTN_StickDirection::UpLeft };

	const float Angle = NormalizeAngle(AngleDeg);

	// Distance to the nearest cardinal axis, in [0, 45]. Beyond (45 - DiagonalHalfAngle) the angle is in a diagonal sector.
	const int32 NearestAxis = FMath::RoundToInt(Angle / 90.f);
	const float DistanceToAxis = FMath::Abs(Angle - NearestAxis * 90.f);

	if (DistanceToAxis <= 45.f - DiagonalHalfAngle)
	{
		return Cardinals[NearestAxis % 4];
	}
	return Diagonals[FMath::FloorToInt(Angle / 90.f) % 4];
}

float FGSTN_DirectionClassifier::GetStickAngle(const FVector2D& RawStick)
{
	if (RawStick.IsNearlyZero(UE_DOUBLE_SMALL_NUMBER))
	{
		return 0.f;
	}
	return NormalizeAngle(FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(RawStick.X), static_cast<float>(RawStick.Y))));
}

float FGSTN_DirectionClassifier::DequantizeStickAxis(int8 Value)
{
	// -128 is never produced by QuantizeStickAxis; clamp it so both signs have the same range
	return FMath::Max(static_cast<int32>(Value), -127) / 127.f;
}

int8 FGSTN_DirectionClassifier::QuantizeStickAxis(float Value)
{
	return static_cast<int8>(FMath::Clamp(FMath::RoundToInt(Value * 127.f), -127, 127));
}

float FGSTN_DirectionClassifier::DequantizeYaw(uint8 Value)
{
	return Value * (360.f / 256.f);
}

uint8 FGSTN_DirectionClassifier::QuantizeYaw(float YawDeg)
{
	return static_cast<uint8>(FMath::RoundToInt(NormalizeAngle(YawDeg) * (256.f / 360.f)) & 0xFF);
}
