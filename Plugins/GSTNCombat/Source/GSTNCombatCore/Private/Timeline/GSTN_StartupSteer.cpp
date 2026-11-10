#include "Timeline/GSTN_StartupSteer.h"

#include "Timeline/GSTN_TimelinePhases.h"
#include "Timeline/GSTN_TimelineTypes.h"

float FGSTN_StartupSteer::SteerYaw(float StartYawDeg, const FVector2D& AimStick, float CameraYawDeg, float MaxSteerDegrees)
{
	const float Start = FRotator::NormalizeAxis(StartYawDeg);
	if (MaxSteerDegrees <= 0.f || AimStick.SizeSquared() < static_cast<double>(DeadZone * DeadZone))
	{
		return Start;
	}

	// Stick right (+X) with the camera at yaw 0 points along world +Y, which is yaw +90: hence atan2(X, Y)
	const float StickYaw = CameraYawDeg + FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(AimStick.X, AimStick.Y)));
	const float Delta = FMath::FindDeltaAngleDegrees(Start, StickYaw);
	return FRotator::NormalizeAxis(Start + FMath::Clamp(Delta, -MaxSteerDegrees, MaxSteerDegrees));
}

bool FGSTN_StartupSteer::IsSteering(const FVector2D& AimStick, float MaxSteerDegrees)
{
	return MaxSteerDegrees > 0.f && AimStick.SizeSquared() >= static_cast<double>(DeadZone * DeadZone);
}

int32 FGSTN_StartupSteer::GetSteerEndFrame(const FGSTN_CombatTimelineData& Data)
{
	int32 First = MAX_int32;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Instance.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (Track != nullptr && Track->EndFrame > Track->StartFrame && FGSTN_TimelinePhases::CountsAsActive(*Track))
		{
			First = FMath::Min(First, FMath::Max(Track->StartFrame, 0));
		}
	}
	return First == MAX_int32 ? 0 : First;
}

int16 FGSTN_StartupSteer::QuantizeYaw(float YawDeg)
{
	const int32 Steps = FMath::RoundToInt(FRotator::NormalizeAxis(YawDeg) * (65536.f / 360.f));
	return static_cast<int16>(static_cast<uint16>(Steps & 0xFFFF));
}

float FGSTN_StartupSteer::DequantizeYaw(int16 Value)
{
	return static_cast<float>(Value) * (360.f / 65536.f);
}
