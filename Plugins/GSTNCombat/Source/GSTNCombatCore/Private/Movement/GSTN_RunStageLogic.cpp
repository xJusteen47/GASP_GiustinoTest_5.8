// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_RunStageTypes.h"

#include "Serialization/Archive.h"

namespace
{
	constexpr int32 MaxCounter = 1023;

	int32 Advance(int32 Counter)
	{
		return Counter < 0 ? Counter : FMath::Min(Counter + 1, MaxCounter);
	}

	void SerializeCounter(FArchive& Ar, int32& Counter)
	{
		// -1..1023 in 11 bits worth of a uint16 (+1 so -1 fits)
		uint16 Packed = static_cast<uint16>(FMath::Clamp(Counter, -1, MaxCounter) + 1);
		Ar << Packed;
		Counter = static_cast<int32>(Packed) - 1;
	}

	void SerializeSpeed(FArchive& Ar, float& Speed)
	{
		int32 Tenths = FMath::RoundToInt(Speed * 10.f);
		Ar << Tenths;
		Speed = Tenths * 0.1f;
	}

	void ResetToFirstStage(FGSTN_RunStageState& State)
	{
		State.Stage = 0;
		State.FramesInStage = 0;
	}
}

FGSTN_RunStagesTuning FGSTN_RunStagesTuning::MakeDefault()
{
	FGSTN_RunStagesTuning Tuning;

	// Starting values only, to be tuned in the profile asset by the owner: stage 1 = today's run, 2 = sprint, 3 = a top speed over it
	FGSTN_RunStage Stage1;
	Stage1.MaxSpeed = 450.f;
	Stage1.Acceleration = 1800.f;
	Stage1.TurningStrength = 12.f;
	Stage1.ReleaseBrake = 4000.f;
	Stage1.FramesToNextStage = 40;
	Stage1.CarryFrames = 3;
	Stage1.AirControlScale = 1.f;

	FGSTN_RunStage Stage2;
	Stage2.MaxSpeed = 650.f;
	Stage2.Acceleration = 1400.f;
	Stage2.TurningStrength = 8.f;
	Stage2.ReleaseBrake = 2500.f;
	Stage2.FramesToNextStage = 60;
	Stage2.CarryFrames = 5;
	Stage2.AirControlScale = 0.8f;

	FGSTN_RunStage Stage3;
	Stage3.MaxSpeed = 850.f;
	Stage3.Acceleration = 1200.f;
	Stage3.TurningStrength = 5.f;
	Stage3.ReleaseBrake = 1800.f;
	Stage3.FramesToNextStage = 1;
	Stage3.CarryFrames = 8;
	Stage3.AirControlScale = 0.6f;

	Tuning.Stages = { Stage1, Stage2, Stage3 };
	return Tuning;
}

bool FGSTN_RunStageState::operator==(const FGSTN_RunStageState& Other) const
{
	return Stage == Other.Stage && FramesInStage == Other.FramesInStage && FramesSinceMoveStart == Other.FramesSinceMoveStart
		&& FramesSinceStickRelease == Other.FramesSinceStickRelease && CarriedSpeed == Other.CarriedSpeed && FramesSinceCarry == Other.FramesSinceCarry
		&& CarrySource == Other.CarrySource && PrevSpeed == Other.PrevSpeed && LastFrame == Other.LastFrame && CarryWindow == Other.CarryWindow
		&& bAirborne == Other.bAirborne && TakeoffSpeed == Other.TakeoffSpeed && TakeoffStage == Other.TakeoffStage
		&& RampFrames == Other.RampFrames && RampStage == Other.RampStage;
}

void FGSTN_RunStageState::SerializeCompact(FArchive& Ar)
{
	Ar << Stage;
	SerializeCounter(Ar, FramesInStage);
	SerializeCounter(Ar, FramesSinceMoveStart);
	SerializeCounter(Ar, FramesSinceStickRelease);
	SerializeCounter(Ar, FramesSinceCarry);
	uint8 Source = static_cast<uint8>(CarrySource);
	Ar << Source;
	CarrySource = static_cast<EGSTN_RunCarrySource>(Source);
	SerializeSpeed(Ar, CarriedSpeed);
	SerializeSpeed(Ar, PrevSpeed);
	Ar << LastFrame;
	SerializeCounter(Ar, CarryWindow);
	uint8 AirBit = bAirborne ? 1 : 0;
	Ar.SerializeBits(&AirBit, 1);
	bAirborne = AirBit != 0;
	SerializeSpeed(Ar, TakeoffSpeed);
	Ar << TakeoffStage;
	SerializeCounter(Ar, RampFrames);
	Ar << RampStage;
}

void FGSTN_RunStageLogic::Step(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunFrameInput& Input, FGSTN_RunStageState& State)
{
	if (!Tuning.IsEnabled() || Input.Frame == State.LastFrame)
	{
		return;
	}
	State.LastFrame = Input.Frame;
	State.FramesSinceCarry = Advance(State.FramesSinceCarry);

	const float Speed = static_cast<float>(Input.GroundVelocity.Size());
	const float PrevSpeed = State.PrevSpeed;
	State.PrevSpeed = Speed;

	// In the air the stage is kept as it was (the landing keeps the run); the counters wait. The first air frame latches the takeoff:
	// the ground speed of the last grounded frame and its stage set the air top speed and steering (P2, decision 1C)
	if (!Input.bGrounded)
	{
		if (!State.bAirborne)
		{
			State.bAirborne = true;
			State.TakeoffSpeed = FMath::Max(PrevSpeed, Speed);
			State.TakeoffStage = State.Stage;
		}
		return;
	}
	State.bAirborne = false;

	// Acceleration ramp: a push from rest starts the ramp of the current stage, a new stage starts its own, the stick at rest ends it
	// (read by GetAccelerationRamp from the next frame's start state; counted before the early returns below so every grounded frame counts)
	if (Input.StickWorld.Size() < Tuning.RampStickThreshold)
	{
		State.RampFrames = -1;
	}
	else if (State.RampFrames < 0 || State.RampStage != State.Stage)
	{
		State.RampFrames = 0;
		State.RampStage = State.Stage;
	}
	else
	{
		State.RampFrames = Advance(State.RampFrames);
	}

	// A skid of this frame (P2): the speed of before it is remembered for the attacks (P3), the run drops to stage 1
	if (Input.bSkidStarted)
	{
		State.CarriedSpeed = Input.SkidStartSpeed;
		State.FramesSinceCarry = 0;
		State.CarrySource = EGSTN_RunCarrySource::Skid;
		State.CarryWindow = Tuning.Skid.CarryFrames;
		if (Tuning.Skid.bResetsStage)
		{
			ResetToFirstStage(State);
		}
		State.FramesInStage = 0;
		return;
	}

	const float Factor = FMath::Max(Input.SpeedFactor, 0.f);
	const float StickSize = static_cast<float>(Input.StickWorld.Size());
	const bool bStickActive = StickSize >= Tuning.StickDeadzone;
	const int32 LastStage = Tuning.Stages.Num() - 1;

	if (bStickActive)
	{
		State.FramesSinceMoveStart = State.FramesSinceMoveStart < 0 ? 0 : Advance(State.FramesSinceMoveStart);
		State.FramesSinceStickRelease = -1;
	}
	else
	{
		if (State.FramesSinceStickRelease < 0)
		{
			// Decision 2A: the speed of before the release, not the one the release brake already ate
			State.FramesSinceStickRelease = 0;
			State.CarriedSpeed = PrevSpeed;
			State.FramesSinceCarry = 0;
			State.CarrySource = EGSTN_RunCarrySource::StickRelease;
			State.CarryWindow = Tuning.GetStage(State.Stage).CarryFrames;
		}
		else
		{
			State.FramesSinceStickRelease = Advance(State.FramesSinceStickRelease);
		}
		State.FramesSinceMoveStart = -1;
		State.FramesInStage = 0;
		if (State.FramesSinceStickRelease >= Tuning.ReleaseDropFrames)
		{
			ResetToFirstStage(State);
		}
		return;
	}

	// Boost: straight to the last stage (owner decision 3C)
	if (Input.bBoostCommand && State.Stage < LastStage)
	{
		const bool bAllowed = Tuning.BoostMode == EGSTN_RunBoostMode::Anytime
			|| (Tuning.BoostMode == EGSTN_RunBoostMode::InitialWindow && State.FramesSinceMoveStart <= Tuning.BoostWindowFrames);
		if (bAllowed)
		{
			State.Stage = static_cast<uint8>(LastStage);
			State.FramesInStage = 0;
			return;
		}
	}

	// Bump: most of the speed gone in one frame with the stick still pushed
	if (PrevSpeed >= Tuning.BumpMinSpeed * Factor && PrevSpeed > KINDA_SMALL_NUMBER && Speed < PrevSpeed * Tuning.BumpSpeedKeptFraction)
	{
		ResetToFirstStage(State);
		return;
	}

	const FVector StickDir = Input.StickWorld / StickSize;
	const bool bMoving = Speed > KINDA_SMALL_NUMBER;
	const float Cos = bMoving ? static_cast<float>(StickDir.Dot(Input.GroundVelocity / Speed)) : 1.f;

	if (bMoving && Cos < Tuning.TightTurnCos)
	{
		if (State.Stage > 0)
		{
			State.Stage = static_cast<uint8>(FMath::Max(0, State.Stage - Tuning.TightTurnDropStages));
		}
		State.FramesInStage = 0;
		return;
	}

	const FGSTN_RunStage& Current = Tuning.GetStage(State.Stage);
	const bool bQualifies = State.Stage < LastStage && Cos >= Tuning.ForwardStickCos
		&& Speed >= Current.MaxSpeed * Factor * Current.NextStageSpeedFraction && Factor > 0.f;
	if (!bQualifies)
	{
		State.FramesInStage = 0;
		return;
	}
	if (++State.FramesInStage >= Current.FramesToNextStage)
	{
		++State.Stage;
		State.FramesInStage = 0;
	}
}

float FGSTN_RunStageLogic::GetCarriedSpeed(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
{
	if (!Tuning.IsEnabled() || State.FramesSinceCarry < 0 || State.CarrySource == EGSTN_RunCarrySource::None)
	{
		return 0.f;
	}
	return State.FramesSinceCarry <= State.CarryWindow ? State.CarriedSpeed : 0.f;
}

float FGSTN_RunStageLogic::GetAirTopSpeed(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, float Factor)
{
	return FMath::Max(State.TakeoffSpeed, Tuning.Air.MinAirSpeed * FMath::Max(Factor, 0.f));
}

float FGSTN_RunStageLogic::GetAirControlScale(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
{
	if (!Tuning.IsEnabled() || Tuning.Air.Model != EGSTN_AirControlModel::TakeoffCap)
	{
		return 1.f;
	}
	return Tuning.GetStage(State.TakeoffStage).AirControlScale;
}

float FGSTN_RunStageLogic::GetAccelerationRamp(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
{
	if (!Tuning.IsEnabled())
	{
		return 1.f;
	}
	const FGSTN_RunStage& Stage = Tuning.GetStage(State.Stage);
	if (Stage.AccelRampFrames <= 0)
	{
		return 1.f;
	}
	// At rest, or a stage the ramp has not caught up with yet (the stage changed at the end of the last frame): the ramp's first frame
	const int32 Frames = (State.RampFrames < 0 || State.RampStage != State.Stage) ? 0 : State.RampFrames + 1;
	const float Alpha = FMath::Clamp(static_cast<float>(Frames) / static_cast<float>(Stage.AccelRampFrames), 0.f, 1.f);
	return FMath::Lerp(FMath::Clamp(Stage.RampStartFraction, 0.f, 1.f), 1.f, Alpha);
}