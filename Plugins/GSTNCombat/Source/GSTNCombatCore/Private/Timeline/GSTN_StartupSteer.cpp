#include "Timeline/GSTN_StartupSteer.h"

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
