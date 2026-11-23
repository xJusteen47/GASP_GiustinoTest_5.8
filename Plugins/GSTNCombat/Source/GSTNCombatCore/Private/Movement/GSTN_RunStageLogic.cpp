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
		State.EntryFrames = -1;
	}

	/** Frames the current stage has counted towards the next one, 0 when not qualifying; -1 when it cannot climb (last stage, air) */
	int32 RunStageFramesTowardsNext(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
	{
		if (!Tuning.IsEnabled() || State.bAirborne || State.Stage >= Tuning.Stages.Num() - 1)
		{
			return -1;
		}
		return State.FramesInStage;
	}

	/** The last Window frames before the change: FramesInStage runs 1..FramesToNextStage-1 while qualifying (the change resets it) */
	bool IsInLastRunStageFrames(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, int32 Window)
	{
		const int32 Counted = RunStageFramesTowardsNext(Tuning, State);
		if (Window <= 0 || Counted <= 0)
		{
			return false;
		}
		return Counted >= Tuning.GetStage(State.Stage).FramesToNextStage - Window;
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
	return Stage == Other.Stage && EntryFrames == Other.EntryFrames && StageEnteredFrame == Other.StageEnteredFrame && FramesInStage == Other.FramesInStage && FramesSinceMoveStart == Other.FramesSinceMoveStart
		&& FramesSinceStickRelease == Other.FramesSinceStickRelease && CarriedSpeed == Other.CarriedSpeed && FramesSinceCarry == Other.FramesSinceCarry
		&& CarrySource == Other.CarrySource && PrevSpeed == Other.PrevSpeed && LastFrame == Other.LastFrame && CarryWindow == Other.CarryWindow
		&& bAirborne == Other.bAirborne && TakeoffSpeed == Other.TakeoffSpeed && TakeoffStage == Other.TakeoffStage
		&& RampFrames == Other.RampFrames && RampStage == Other.RampStage && StampFrames == Other.StampFrames && DashFrames == Other.DashFrames && ReverseFrames == Other.ReverseFrames && ExternalVelocity == Other.ExternalVelocity;
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
	SerializeCounter(Ar, StampFrames);
	SerializeCounter(Ar, DashFrames);
	SerializeCounter(Ar, ReverseFrames);
	SerializeCounter(Ar, EntryFrames);
	Ar << StageEnteredFrame;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		float Component = static_cast<float>(ExternalVelocity[Axis]);
		SerializeSpeed(Ar, Component);
		ExternalVelocity[Axis] = Component;
	}
}

// The stage rules of one frame (Step wraps it to stamp the frame a stage began, whichever of the returns below changed it)
static void StepRunStages(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunFrameInput& Input, FGSTN_RunStageState& State)
{
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
	const float RampStick = static_cast<float>(Input.StickWorld.Size());
	if (RampStick < Tuning.RampStickThreshold)
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

	// Run start stamp (owner choice B): counted like the ramp, before the early returns. Starts once per push, from (almost) still, on stage 1,
	// when the stick is pushed hard enough; the stage changing (boost) or the stick resting ends it
	{
		const FGSTN_RunStartStampTuning& Stamp = Tuning.StartStamp;
		if (Stamp.Frames <= 0 || RampStick < Tuning.RampStickThreshold)
		{
			State.StampFrames = -1;
		}
		else if (State.StampFrames >= 0)
		{
			State.StampFrames = Advance((State.Stage == 0) ? State.StampFrames : FMath::Max(State.StampFrames, Stamp.Frames));
		}
		else if (State.Stage == 0 && RampStick >= Stamp.StickMin && PrevSpeed < Stamp.MaxStartSpeed)
		{
			// Frame 0 of the stamp is this frame: GetAccelerationRamp already cut its acceleration (stick at rest at its start, see there)
			State.StampFrames = 0;
		}
	}
	// Run reversal (skid-reversal-1): starts on the frame the stick goes against a run, ends once the run the other way is back. Counted before
	// the early returns (skid, stick release) like the ramp, so the multiplier never sticks
	{
		const FGSTN_RunReversalTuning& Rev = Tuning.Reversal;
		if (!Rev.bEnabled || RampStick < FMath::Max(Rev.MinStickMagnitude, KINDA_SMALL_NUMBER))
		{
			State.ReverseFrames = -1;
		}
		else
		{
			const float RevFactor = FMath::Max(Input.SpeedFactor, 0.f);
			const float Along = static_cast<float>((Input.StickWorld / RampStick).Dot(Input.GroundVelocity));
			if (State.ReverseFrames < 0)
			{
				if (Speed > KINDA_SMALL_NUMBER && Speed >= Rev.MinSpeed * RevFactor && Along / Speed < Rev.StickCos)
				{
					State.ReverseFrames = 0;
				}
			}
			else
			{
				State.ReverseFrames = Advance(State.ReverseFrames);
				const float EndSpeed = Rev.EndSpeedFraction * Tuning.GetStage(State.Stage).MaxSpeed * RevFactor;
				if (Along >= EndSpeed || State.ReverseFrames >= Rev.MaxFrames)
				{
					State.ReverseFrames = -1;
				}
			}
		}
	}

	// Boost dash counter (set by the boost below), reset with the stick at rest
	State.DashFrames = RampStick < Tuning.RampStickThreshold ? -1 : Advance(State.DashFrames);
	// Readable step's entry burst counter (set by the climb below), reset with the stick at rest like the dash
	State.EntryFrames = RampStick < Tuning.RampStickThreshold ? -1 : Advance(State.EntryFrames);

	// A skid of this frame (P2): the speed of before it is remembered for the attacks (P3), the run drops to stage 1
	if (Input.bSkidStarted)
	{
		State.CarriedSpeed = Input.SkidStartSpeed;
		State.FramesSinceCarry = 0;
		State.CarrySource = EGSTN_RunCarrySource::Skid;
		State.CarryWindow = Tuning.Skid.CarryFrames;
		State.EntryFrames = -1;
		if (Tuning.Skid.bResetsStage && !Tuning.Skid.bResumeStageAfterSkid)
		{
			ResetToFirstStage(State);
		}
		State.FramesInStage = 0;
		return;
	}

	// Resume after the skid (owner 2026-11-17): the stage is frozen while the skid lives, so the push off continues from it. Without this the
	// reversed stick reads as a tight turn every skid frame. The stage counters restart from zero once the skid is over
	if (Input.bSkidActive && Tuning.Skid.bResumeStageAfterSkid)
	{
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

	// Boost: straight to BoostTargetStage (owner decision 3C; owner 2026-11-17: the middle stage by default). It never lowers the stage
	const int32 BoostTarget = Tuning.GetBoostTargetIndex();
	if (Input.bBoostCommand && State.Stage < BoostTarget)
	{
		// The initial window also covers the whole start stamp: L3 is how the owner skips it (2026-11-19)
		const bool bAllowed = Tuning.BoostMode == EGSTN_RunBoostMode::Anytime
			|| (Tuning.BoostMode == EGSTN_RunBoostMode::InitialWindow
				&& (State.FramesSinceMoveStart <= Tuning.BoostWindowFrames || FGSTN_RunStageLogic::IsStartStamp(Tuning, State)));
		if (bAllowed)
		{
			State.Stage = static_cast<uint8>(BoostTarget);
			State.FramesInStage = 0;
			// Smash-style dash: the stamp is over (its clip leaves), the burst starts
			State.StampFrames = -1;
			State.DashFrames = Tuning.BoostDash.Frames > 0 ? 0 : -1;
			State.EntryFrames = -1;	// the dash is the boost's burst; the readable step's is for a climb only
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
		State.EntryFrames = -1;
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
		// Readable step (decision 1A): the climb starts the new stage's burst, in the frame the clip and the effects change
		State.EntryFrames = (Input.bReadableSteps && Tuning.GetStage(State.Stage).EntryBurstFrames > 0) ? 0 : -1;
	}
}

void FGSTN_RunStageLogic::Step(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunFrameInput& Input, FGSTN_RunStageState& State)
{
	if (!Tuning.IsEnabled() || Input.Frame == State.LastFrame)
	{
		return;
	}
	const uint8 StageBefore = State.Stage;
	StepRunStages(Tuning, Input, State);
	if (State.Stage != StageBefore)
	{
		State.StageEnteredFrame = Input.Frame;
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

bool FGSTN_RunStageLogic::IsStartStamp(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
{
	return Tuning.IsEnabled() && Tuning.StartStamp.Frames > 0 && State.Stage == 0 && !State.bAirborne
		&& State.StampFrames >= 0 && State.StampFrames < Tuning.StartStamp.Frames;
}

bool FGSTN_RunStageLogic::IsReversing(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
{
	return Tuning.IsEnabled() && Tuning.Reversal.bEnabled && !State.bAirborne && State.ReverseFrames >= 0;
}

bool FGSTN_RunStageLogic::IsBoostDash(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State)
{
	return Tuning.IsEnabled() && Tuning.BoostDash.Frames > 0 && State.DashFrames >= 0 && State.DashFrames < Tuning.BoostDash.Frames;
}

bool FGSTN_RunStageLogic::IsStageEntryBurst(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps)
{
	return bReadableSteps && Tuning.IsEnabled() && !State.bAirborne && State.EntryFrames >= 0
		&& State.EntryFrames < Tuning.GetStage(State.Stage).EntryBurstFrames;
}

bool FGSTN_RunStageLogic::IsStageTell(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps)
{
	return bReadableSteps && Tuning.IsEnabled() && IsInLastRunStageFrames(Tuning, State, Tuning.GetStage(State.Stage).TellFrames);
}

float FGSTN_RunStageLogic::GetStageTellAlpha(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps)
{
	if (!IsStageTell(Tuning, State, bReadableSteps))
	{
		return 0.f;
	}
	const FGSTN_RunStage& Stage = Tuning.GetStage(State.Stage);
	// The tell window's frames counted from 1; a window longer than the stage starts at the first qualifying frame
	const int32 Window = FMath::Min(Stage.TellFrames, FMath::Max(Stage.FramesToNextStage - 1, 1));
	const int32 Into = State.FramesInStage - (Stage.FramesToNextStage - 1 - Window);
	return FMath::Clamp(static_cast<float>(Into) / static_cast<float>(Window), 0.f, 1.f);
}

bool FGSTN_RunStageLogic::IsStageCharge(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps)
{
	return bReadableSteps && Tuning.IsEnabled() && IsInLastRunStageFrames(Tuning, State, Tuning.GetStage(State.Stage).ChargeFrames);
}

float FGSTN_RunStageLogic::GetAccelerationRamp(const FGSTN_RunStagesTuning& Tuning, const FGSTN_RunStageState& State, bool bReadableSteps)
{
	if (!Tuning.IsEnabled())
	{
		return 1.f;
	}
	if (IsBoostDash(Tuning, State))
	{
		return FMath::Clamp(Tuning.BoostDash.AccelMultiplier, 1.f, 30.f);
	}
	if (IsStageEntryBurst(Tuning, State, bReadableSteps))
	{
		return FMath::Clamp(Tuning.GetStage(State.Stage).EntryAccelMultiplier, 1.f, 30.f);
	}
	if (IsReversing(Tuning, State))
	{
		// No ramp and no stamp cut: the stage drop of the tight turn would otherwise restart the ramp from its start fraction
		return FMath::Clamp(Tuning.Reversal.AccelerationMultiplier, 1.f, 10.f);
	}
	const FGSTN_RunStartStampTuning& Stamp = Tuning.StartStamp;
	if (Stamp.Frames > 0 && State.Stage == 0 && !State.bAirborne)
	{
		// The stamp's frames 1..Frames-1 (the counter of the start state is 0..Frames-2), and frame 0: the stick was at rest at the start of
		// the frame and the character still, so a push on this frame may start a stamp. A light tilt then loses one frame of acceleration
		// only (the stamp does not start, the next frame ramps as usual); a full push gets no one-frame jump before the stamp
		const bool bStampNext = State.StampFrames >= 0 && State.StampFrames + 1 < Stamp.Frames;
		const bool bMaybeFirst = State.RampFrames < 0 && State.StampFrames < 0 && State.PrevSpeed < Stamp.MaxStartSpeed;
		if (bStampNext || bMaybeFirst)
		{
			return FMath::Clamp(Stamp.AccelFraction, 0.f, 1.f);
		}
	}
	const FGSTN_RunStage& Stage = Tuning.GetStage(State.Stage);
	if (Stage.AccelRampFrames <= 0)
	{
		return 1.f;
	}
	if (bReadableSteps && State.Stage > 0 && Stage.EntryBurstFrames > 0)
	{
		// Decision 1A: the ramp is only for the start from rest; a stage with a step has none (after its burst, and on a boost or a drop into it)
		return 1.f;
	}
	// At rest, or a stage the ramp has not caught up with yet (the stage changed at the end of the last frame): the ramp's first frame
	int32 Frames = (State.RampFrames < 0 || State.RampStage != State.Stage) ? 0 : State.RampFrames + 1;
	if (Stamp.Frames > 0 && State.Stage == 0 && State.StampFrames >= 0 && State.StampFrames <= State.RampFrames)
	{
		// Only for the ramp the stamp belongs to (both started on the push, the stamp on the same frame or later). A ramp restarted later
		// (the tight turn's drop back to stage 1) is younger than the stamp counter and must not wait Stamp.Frames again (skid-reversal-1:
		// it held the reversal at the ramp's start fraction for 21 frames)
		// After a stamp the stage 1 ramp starts where the stamp ended, not at the push: no acceleration step out of the stamp
		Frames = FMath::Max(0, Frames - Stamp.Frames);
	}
	const float Alpha = FMath::Clamp(static_cast<float>(Frames) / static_cast<float>(Stage.AccelRampFrames), 0.f, 1.f);
	return FMath::Lerp(FMath::Clamp(Stage.RampStartFraction, 0.f, 1.f), 1.f, Alpha);
}