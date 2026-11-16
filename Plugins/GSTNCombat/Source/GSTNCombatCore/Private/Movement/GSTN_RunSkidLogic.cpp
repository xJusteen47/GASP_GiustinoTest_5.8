// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_RunSkidTypes.h"

bool FGSTN_RunSkidMath::ShouldStart(const FGSTN_RunSkidTuning& Tuning, int32 Stage, const FVector& GroundVelocity, const FVector& StickWorld, float Factor)
{
	if (!Tuning.bEnabled || Stage < Tuning.MinStage || Factor <= 0.f)
	{
		return false;
	}
	const double Speed = GroundVelocity.Size();
	const double Stick = StickWorld.Size();
	if (Speed < Tuning.MinSpeed * Factor || Speed <= KINDA_SMALL_NUMBER || Stick < Tuning.MinStickMagnitude || Stick <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	return (StickWorld / Stick).Dot(GroundVelocity / Speed) <= Tuning.ReverseStickCos;
}

float FGSTN_RunSkidMath::SpeedAt(float StartSpeed, float Brake, int32 LiveFrame, float DeltaSeconds)
{
	return FMath::Max(0.f, StartSpeed - Brake * DeltaSeconds * static_cast<float>(FMath::Max(LiveFrame, 0)));
}

int32 FGSTN_RunSkidMath::FramesToRest(float StartSpeed, float Brake, float DeltaSeconds)
{
	const float PerFrame = Brake * DeltaSeconds;
	return PerFrame > 0.f ? FMath::CeilToInt32(StartSpeed / PerFrame) : MAX_int32;
}

EGSTN_RunSkidPhase FGSTN_RunSkidMath::PhaseAt(const FGSTN_RunSkidTuning& Tuning, int32 LiveFrame, int32 RestFrame)
{
	if (LiveFrame < 0)
	{
		return EGSTN_RunSkidPhase::None;
	}
	if (LiveFrame < RestFrame)
	{
		return LiveFrame < Tuning.EnterFrames ? EGSTN_RunSkidPhase::Enter : EGSTN_RunSkidPhase::Active;
	}
	return LiveFrame < RestFrame + Tuning.ExitFrames ? EGSTN_RunSkidPhase::Exit : EGSTN_RunSkidPhase::None;
}

bool FGSTN_RunSkidMath::InJumpWindow(const FGSTN_RunSkidTuning& Tuning, int32 LiveFrame)
{
	// The abilities read this frame's tag on the next frame: the window is shifted one frame early so it opens on the authored frame for them
	const int32 Seen = LiveFrame + 1;
	return Seen >= Tuning.JumpWindowStart && (Tuning.JumpWindowEnd < 0 || Seen <= Tuning.JumpWindowEnd);
}

float FGSTN_RunSkidMath::StopDistance(float StartSpeed, float Brake)
{
	return Brake > 0.f ? StartSpeed * StartSpeed / (2.f * Brake) : 0.f;
}
