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

// skid-physics-1: the Physics motion. Integrated frame by frame (explicit Euler at the fixed tick) instead of a closed form because the
// shapes are designer curves; a skid is a few dozen frames, so recomputing from frame 0 each step costs nothing and keeps the move stateless

float FGSTN_RunSkidMath::EvalShape(const FRuntimeFloatCurve& Curve, float X)
{
	const FRichCurve* Rich = Curve.GetRichCurveConst();
	if (!Rich || Rich->GetNumKeys() == 0)
	{
		return 1.f;
	}
	return FMath::Max(Rich->Eval(X, 1.f), 0.05f);
}

namespace GSTNRunSkidPhysics
{
	/** One brake frame (0-based) from speed Speed: the onset ramps the brake in over the first frames */
	float BrakeStep(const FGSTN_RunSkidPhysicsTuning& Physics, float StartSpeed, float BrakeDecel, float Speed, int32 Frame, float DeltaSeconds)
	{
		const float Onset = Physics.BrakeOnsetFrames > 0
			? FMath::Min(static_cast<float>(Frame + 1) / static_cast<float>(Physics.BrakeOnsetFrames + 1), 1.f) : 1.f;
		const float Left = StartSpeed > KINDA_SMALL_NUMBER ? FMath::Clamp(Speed / StartSpeed, 0.f, 1.f) : 0.f;
		return FMath::Max(Speed - BrakeDecel * Onset * FGSTN_RunSkidMath::EvalShape(Physics.BrakeCurve, Left) * DeltaSeconds, 0.f);
	}

	float PushStep(const FGSTN_RunSkidPhysicsTuning& Physics, float PushAccel, float PushSpeed, float Speed, float DeltaSeconds)
	{
		const float Share = PushSpeed > KINDA_SMALL_NUMBER ? FMath::Clamp(Speed / PushSpeed, 0.f, 1.f) : 1.f;
		return FMath::Min(Speed + PushAccel * FGSTN_RunSkidMath::EvalShape(Physics.PushCurve, Share) * DeltaSeconds, PushSpeed);
	}
}

float FGSTN_RunSkidMath::PhysicsBrakeSpeedAt(const FGSTN_RunSkidPhysicsTuning& Physics, float StartSpeed, float BrakeDecel, int32 LiveFrame, float DeltaSeconds)
{
	if (LiveFrame < 0)
	{
		return StartSpeed;
	}
	const int32 Last = FMath::Min(LiveFrame, FMath::Max(Physics.MaxFrames, 1) - 1);
	float Speed = StartSpeed;
	for (int32 Frame = 0; Frame <= Last && Speed > 0.f; ++Frame)
	{
		Speed = GSTNRunSkidPhysics::BrakeStep(Physics, StartSpeed, BrakeDecel, Speed, Frame, DeltaSeconds);
	}
	// Past the cap the skid is at rest whatever is left (a wall or a slope fought it)
	return LiveFrame >= FMath::Max(Physics.MaxFrames, 1) - 1 ? 0.f : Speed;
}

int32 FGSTN_RunSkidMath::PhysicsFramesToRest(const FGSTN_RunSkidPhysicsTuning& Physics, float StartSpeed, float BrakeDecel, float DeltaSeconds)
{
	const int32 Cap = FMath::Max(Physics.MaxFrames, 1);
	float Speed = StartSpeed;
	int32 Frames = 0;
	while (Speed > 0.f && Frames < Cap)
	{
		Speed = GSTNRunSkidPhysics::BrakeStep(Physics, StartSpeed, BrakeDecel, Speed, Frames, DeltaSeconds);
		++Frames;
	}
	return FMath::Max(Frames, 1);
}

float FGSTN_RunSkidMath::PhysicsPushSpeedAt(const FGSTN_RunSkidPhysicsTuning& Physics, float PushAccel, float PushSpeed, int32 PushFrame, float DeltaSeconds)
{
	if (PushSpeed <= 0.f || PushFrame < 0)
	{
		return 0.f;
	}
	const int32 Last = FMath::Min(PushFrame, FMath::Max(Physics.MaxFrames, 1) - 1);
	float Speed = 0.f;
	for (int32 Frame = 0; Frame <= Last && Speed < PushSpeed; ++Frame)
	{
		Speed = GSTNRunSkidPhysics::PushStep(Physics, PushAccel, PushSpeed, Speed, DeltaSeconds);
	}
	return PushFrame >= FMath::Max(Physics.MaxFrames, 1) - 1 ? PushSpeed : Speed;
}

int32 FGSTN_RunSkidMath::PhysicsPushFrames(const FGSTN_RunSkidPhysicsTuning& Physics, float PushAccel, float PushSpeed, float DeltaSeconds)
{
	if (PushSpeed <= 0.f)
	{
		return 0;
	}
	const int32 Cap = FMath::Max(Physics.MaxFrames, 1);
	float Speed = 0.f;
	int32 Frames = 0;
	while (Speed < PushSpeed && Frames < Cap)
	{
		Speed = GSTNRunSkidPhysics::PushStep(Physics, PushAccel, PushSpeed, Speed, DeltaSeconds);
		++Frames;
	}
	return Frames;
}

float FGSTN_RunSkidMath::PhysicsTurnShare(const FGSTN_RunSkidPhysicsTuning& Physics, float Progress)
{
	const float Start = Physics.TurnStartProgress;
	const float End = Physics.TurnEndProgress;
	if (End <= Start + KINDA_SMALL_NUMBER)
	{
		return Progress >= Start ? 1.f : 0.f;
	}
	return FMath::SmoothStep(Start, End, Progress);
}
