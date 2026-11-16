#include "Timeline/GSTN_StartupSteer.h"

#include "Timeline/GSTN_TimelinePhases.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Curves/RichCurve.h"

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

float FGSTN_StartupSteer::RateLimitYaw(float CurrentYawDeg, float TargetYawDeg, float RateDegPerSecond, float StepSeconds)
{
	if (RateDegPerSecond <= 0.f || StepSeconds <= 0.f)
	{
		return FRotator::NormalizeAxis(TargetYawDeg);
	}
	const float MaxStep = RateDegPerSecond * StepSeconds;
	const float Delta = FRotator::NormalizeAxis(TargetYawDeg - CurrentYawDeg);
	return FRotator::NormalizeAxis(CurrentYawDeg + FMath::Clamp(Delta, -MaxStep, MaxStep));
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

bool FGSTN_StartupSteer::PickStick(EGSTN_SteerSource Source, const FVector2D& MoveStick, const FVector2D& AimStick, bool bPrecisionAim, FVector2D& OutStick)
{
	const bool bAimPushed = AimStick.SizeSquared() >= static_cast<double>(DeadZone * DeadZone);
	switch (Source)
	{
	case EGSTN_SteerSource::MoveStick:
		OutStick = MoveStick;
		return true;
	case EGSTN_SteerSource::MoveOrAimStick:
		OutStick = bPrecisionAim && bAimPushed ? AimStick : MoveStick;
		return true;
	case EGSTN_SteerSource::AimStickWithReticle:
	default:
		OutStick = bPrecisionAim ? AimStick : FVector2D::ZeroVector;
		return bPrecisionAim;
	}
}

bool FGSTN_StartupSteer::IsMoveStickSource(EGSTN_SteerSource Source)
{
	return Source == EGSTN_SteerSource::MoveStick || Source == EGSTN_SteerSource::MoveOrAimStick;
}

void FGSTN_StartupSteer::GetSteerWindow(const FGSTN_CombatTimelineData& Data, int32& OutStart, int32& OutEnd)
{
	OutStart = FMath::Max(Data.Steer.StartFrame, 0);
	OutEnd = Data.Steer.EndFrame >= 0 ? Data.Steer.EndFrame : GetSteerEndFrame(Data);
}

int32 FGSTN_StartupSteer::GetHoldEndFrame(const FGSTN_CombatTimelineData& Data)
{
	int32 Last = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Instance.GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (Track != nullptr && Track->EndFrame > Track->StartFrame && FGSTN_TimelinePhases::CountsAsActive(*Track))
		{
			Last = FMath::Max(Last, Track->EndFrame);
		}
	}
	int32 Start = 0;
	int32 End = 0;
	GetSteerWindow(Data, Start, End);
	return Last > 0 ? FMath::Max(Last, End) : 0;
}

float FGSTN_StartupSteer::RateLimitInCone(float StartYawDeg, float CurrentYawDeg, float TargetYawDeg, float MaxSteerDegrees, float MaxStepDeg)
{
	if (MaxStepDeg <= 0.f)
	{
		return FRotator::NormalizeAxis(TargetYawDeg);
	}
	if (MaxSteerDegrees <= 90.f || MaxSteerDegrees >= 180.f)
	{
		// Two yaws inside a cone of 90 either way are never more than 180 apart, so the short way stays inside it
		const float Delta = FRotator::NormalizeAxis(TargetYawDeg - CurrentYawDeg);
		return FRotator::NormalizeAxis(CurrentYawDeg + FMath::Clamp(Delta, -MaxStepDeg, MaxStepDeg));
	}
	const float CurrentOffset = FMath::Clamp(FMath::FindDeltaAngleDegrees(StartYawDeg, CurrentYawDeg), -MaxSteerDegrees, MaxSteerDegrees);
	const float TargetOffset = FMath::Clamp(FMath::FindDeltaAngleDegrees(StartYawDeg, TargetYawDeg), -MaxSteerDegrees, MaxSteerDegrees);
	return FRotator::NormalizeAxis(StartYawDeg + CurrentOffset + FMath::Clamp(TargetOffset - CurrentOffset, -MaxStepDeg, MaxStepDeg));
}

EGSTN_SteerPhase FGSTN_StartupSteer::ResolveFrame(const FGSTN_CombatTimelineData& Data, const FGSTN_SteerFrameInput& In, float& OutYawDeg, bool& OutStickActive)
{
	OutStickActive = false;
	const float MaxSteer = Data.MaxSteerDegrees;
	if (MaxSteer <= 0.f || In.TimelineFrame < 0)
	{
		return EGSTN_SteerPhase::None;
	}
	const bool bHold = IsMoveStickSource(Data.Steer.Source) && Data.Steer.bHoldFacing;
	int32 WindowStart = 0;
	int32 WindowEnd = 0;
	GetSteerWindow(Data, WindowStart, WindowEnd);
	const float Current = FRotator::NormalizeAxis(In.CurrentYawDeg);

	if (In.TimelineFrame < WindowStart || In.TimelineFrame >= WindowEnd)
	{
		const bool bBefore = In.TimelineFrame < WindowStart;
		if (!bHold || (!bBefore && In.TimelineFrame >= GetHoldEndFrame(Data)))
		{
			return EGSTN_SteerPhase::None;
		}
		OutYawDeg = Current;
		return bBefore ? EGSTN_SteerPhase::Before : EGSTN_SteerPhase::Locked;
	}
	if (!In.bSourceActive)
	{
		// The reticle source with the reticle off: nothing steers, nothing holds (the left stick turns the character as before)
		if (!bHold)
		{
			return EGSTN_SteerPhase::None;
		}
		OutYawDeg = Current;
		return EGSTN_SteerPhase::Steering;
	}

	const float Start = FRotator::NormalizeAxis(In.StartYawDeg);
	OutStickActive = IsSteering(In.Stick, MaxSteer);
	float Target = 0.f;
	if (OutStickActive || !IsMoveStickSource(Data.Steer.Source))
	{
		Target = SteerYaw(Start, In.Stick, In.CameraYawDeg, MaxSteer);
	}
	else
	{
		// Move stick released: the aim stays where it got to (kept inside the cone)
		Target = FRotator::NormalizeAxis(Start + FMath::Clamp(FMath::FindDeltaAngleDegrees(Start, Current), -MaxSteer, MaxSteer));
	}

	if (Data.SteerRateDegPerSecond > 0.f && In.StepSeconds > 0.f)
	{
		float Scale = 1.f;
		const FRichCurve* Curve = Data.Steer.RateScale.GetRichCurveConst();
		if (Curve && Curve->GetNumKeys() > 0)
		{
			const float T = static_cast<float>(In.TimelineFrame - WindowStart) / static_cast<float>(FMath::Max(WindowEnd - WindowStart, 1));
			Scale = FMath::Max(Curve->Eval(T, 1.f), 0.f);
		}
		Target = RateLimitInCone(Start, Current, Target, MaxSteer, Data.SteerRateDegPerSecond * Scale * In.StepSeconds);
	}
	OutYawDeg = FRotator::NormalizeAxis(Target);
	return EGSTN_SteerPhase::Steering;
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
