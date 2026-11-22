// Combat presentation (GAS-NPP fork).

#include "Presentation/GSTN_LocomotionLean.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_LocomotionLean)

namespace GSTNLocomotionLean
{
	/** Larger measured accelerations are a teleport, a respawn or a correction snap, not movement: the solver restarts from the new velocity */
	constexpr float MaxPlausibleAccel = 30000.f;
	/** Longer frames (a hitch) are cut, so the spring cannot blow up */
	constexpr float MaxStepSeconds = 0.05f;
}

FVector FGSTN_LocomotionLeanSolver::TargetLean(const FGSTN_LocomotionLeanSettings& Settings, const FVector& HorizontalVelocity,
	const FVector& HorizontalAccel, const FVector& Forward, float StateScale)
{
	const float Speed = HorizontalVelocity.Size();
	const float SpeedRange = FMath::Max(Settings.FullLeanSpeed - Settings.MinSpeed, 1.f);
	const float SpeedWeight = FMath::Clamp((Speed - Settings.MinSpeed) / SpeedRange, 0.f, 1.f);
	const float Scale = SpeedWeight * StateScale;
	if (Scale <= 0.f || Settings.MaxAngleDeg <= 0.f)
	{
		return FVector::ZeroVector;
	}
	// Split along the heading: along = speeding up / braking, the rest = turning (points to the centre of the turn)
	const FVector Heading = Speed > KINDA_SMALL_NUMBER ? HorizontalVelocity / Speed : Forward.GetSafeNormal2D();
	const float Along = FVector::DotProduct(HorizontalAccel, Heading);
	const FVector Side = HorizontalAccel - Heading * Along;
	const FVector Accel = Heading * (Along * Settings.SpeedChangeScale) + Side * Settings.TurnScale;
	const FVector Target = Accel * (Settings.MaxAngleDeg / FMath::Max(Settings.AccelForMaxLean, 1.f));
	return Target.GetClampedToMaxSize(Settings.MaxAngleDeg) * Scale;
}

FVector FGSTN_LocomotionLeanSolver::Step(const FGSTN_LocomotionLeanSettings& Settings, const FVector& Velocity, const FVector& Forward,
	float DeltaSeconds, float StateScale, float ExtraForwardDeg)
{
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	const float Dt = FMath::Min(DeltaSeconds, GSTNLocomotionLean::MaxStepSeconds);
	if (Dt <= KINDA_SMALL_NUMBER)
	{
		return Lean;
	}
	FVector Accel = FVector::ZeroVector;
	if (bHasPrev)
	{
		Accel = (Horizontal - PrevVelocity) / Dt;
		if (Accel.SizeSquared() > FMath::Square(GSTNLocomotionLean::MaxPlausibleAccel))
		{
			Accel = FVector::ZeroVector;
			FilteredAccel = FVector::ZeroVector;
		}
	}
	PrevVelocity = Horizontal;
	bHasPrev = true;

	// The presented velocity changes in 62.5 Hz steps: a frame-rate independent low pass turns the steps into a steady acceleration
	const float Alpha = Settings.AccelSmoothingSeconds > 0.f ? 1.f - FMath::Exp(-Dt / Settings.AccelSmoothingSeconds) : 1.f;
	FilteredAccel += (Accel - FilteredAccel) * Alpha;

	FVector Target = Settings.bEnabled ? TargetLean(Settings, Horizontal, FilteredAccel, Forward, StateScale) : FVector::ZeroVector;
	if (Settings.bEnabled && ExtraForwardDeg > 0.f)
	{
		// The readable run stages' forward lean: along the run, faded in with speed like the rest (a standing character stays straight)
		const float Speed = Horizontal.Size();
		const float SpeedRange = FMath::Max(Settings.FullLeanSpeed - Settings.MinSpeed, 1.f);
		const float SpeedWeight = FMath::Clamp((Speed - Settings.MinSpeed) / SpeedRange, 0.f, 1.f);
		const FVector Heading = Speed > KINDA_SMALL_NUMBER ? Horizontal / Speed : Forward.GetSafeNormal2D();
		Target += Heading * (ExtraForwardDeg * SpeedWeight);
	}

	// Damped spring (semi-implicit Euler): damping under 1 overshoots a little on the way back, the follow-through of a cartoon run
	// Sub-steps of at most 1/120 s keep it stable at any frame rate and frequency
	const float Omega = 2.f * PI * Settings.SpringFrequency;
	const int32 SubSteps = FMath::Clamp(FMath::CeilToInt(Dt * 120.f), 1, 8);
	const float SubDt = Dt / SubSteps;
	for (int32 i = 0; i < SubSteps; ++i)
	{
		const FVector SpringAccel = (Target - Lean) * (Omega * Omega) - LeanRate * (2.f * Settings.SpringDamping * Omega);
		LeanRate += SpringAccel * SubDt;
		Lean += LeanRate * SubDt;
	}
	Lean.Z = 0.f;
	LeanRate.Z = 0.f;
	// Overshoot may pass the limit a little, never by more than a fifth
	Lean = Lean.GetClampedToMaxSize(Settings.MaxAngleDeg * 1.2f + Settings.GetMaxExtraForwardLeanDeg());
	return Lean;
}

float FGSTN_SkidSectionSettings::ClipTime(EGSTN_RunSkidPhase Phase, int32 Frame, int32 RestFrame, int32 EnterFrames, int32 ExitFrames, float Speed,
	float EntrySpeed, float Length) const
{
	if (Length <= 0.f || Phase == EGSTN_RunSkidPhase::None)
	{
		return 0.f;
	}
	const float HoldStart = Length * FMath::Clamp(HoldStartFraction, 0.f, 1.f);
	const float PushStart = FMath::Max(HoldStart, Length * FMath::Clamp(PushStartFraction, 0.f, 1.f));
	switch (Phase)
	{
	case EGSTN_RunSkidPhase::Enter:
	{
		// Fixed: the cut in reaches the hold on the last enter frame whatever the speed
		const float Progress = EnterFrames > 0 ? FMath::Clamp(static_cast<float>(Frame + 1) / static_cast<float>(EnterFrames), 0.f, 1.f) : 1.f;
		return HoldStart * Progress;
	}
	case EGSTN_RunSkidPhase::Active:
	{
		// Driven by the speed left, not by time: the same pose at the same share of the brake from every run stage
		float Progress = 1.f;
		if (EntrySpeed > KINDA_SMALL_NUMBER)
		{
			Progress = FMath::Clamp(1.f - Speed / EntrySpeed, 0.f, 1.f);
		}
		else if (RestFrame > EnterFrames)
		{
			Progress = FMath::Clamp(static_cast<float>(Frame - EnterFrames) / static_cast<float>(RestFrame - EnterFrames), 0.f, 1.f);
		}
		const float Eased = 1.f - FMath::Pow(1.f - Progress, FMath::Max(HoldEaseOut, 1.f));
		return HoldStart + (PushStart - HoldStart) * Eased;
	}
	case EGSTN_RunSkidPhase::Exit:
	default:
	{
		// Fixed: the push off plays over the exit frames, ease-in
		const int32 AfterRest = FMath::Max(Frame - RestFrame, 0);
		const float Progress = ExitFrames > 0 ? FMath::Clamp(static_cast<float>(AfterRest + 1) / static_cast<float>(ExitFrames), 0.f, 1.f) : 1.f;
		return FMath::Min(PushStart + (Length - PushStart) * FMath::Pow(Progress, FMath::Max(PushEaseIn, 1.f)), Length);
	}
	}
}

void FGSTN_RunStartScrambleState::Step(const FGSTN_RunStartScrambleSettings& Settings, float DeltaSeconds, float Speed, float TargetSpeed, float Stick,
	bool bOnGround, bool bSkidding, bool bHasClip)
{
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, GSTNLocomotionLean::MaxStepSeconds);
	const float StickFactor = Settings.StickFactor(Stick);
	const float EndSpeed = TargetSpeed * Settings.EndSpeedFraction;
	if (bActive)
	{
		if (!bOnGround || bSkidding || Speed < Settings.StartFromSpeed || Speed >= EndSpeed || StickFactor <= 0.f)
		{
			bActive = false;
		}
	}
	else if (Settings.bEnabled && bHasClip && TargetSpeed > 0.f && bOnGround && !bSkidding && StickFactor > 0.f
		&& PreviousSpeed < Settings.StartFromSpeed && Speed >= Settings.StartFromSpeed)
	{
		bActive = true;
		ClipTime = 0.f;
	}
	PreviousSpeed = Speed;

	Alpha = bActive && TargetSpeed > 0.f ? FMath::Clamp(1.f - Speed / TargetSpeed, 0.f, 1.f) * StickFactor : 0.f;
	if (bActive)
	{
		ClipTime += Dt * PlayRate(Settings);
	}

	// The extra lean: a spring on its target (cut in fast, out with one rebound past the run lean)
	const float Target = Settings.ExtraLeanDeg * Alpha;
	const float Omega = 2.f * PI * FMath::Max(Settings.LeanSpringFrequency, 0.1f);
	const float Accel = Omega * Omega * (Target - LeanDeg) - 2.f * FMath::Max(Settings.LeanSpringDamping, 0.f) * Omega * LeanRate;
	LeanRate += Accel * Dt;
	LeanDeg += LeanRate * Dt;
	if (!bActive && FMath::Abs(LeanDeg) < 0.05f && FMath::Abs(LeanRate) < 0.5f)
	{
		LeanDeg = 0.f;
		LeanRate = 0.f;
	}
}

void FGSTN_RunStopTrack::Build(const TArray<FVector>& RootPositions, float InSampleRate)
{
	SampleRate = FMath::Max(InSampleRate, 1.f);
	Speed.Reset();
	RunSpeed = 0.f;
	BrakeStart = StopIndex = 0;
	const int32 Num = RootPositions.Num();
	if (Num < 3)
	{
		return;
	}
	// Horizontal root speed, smoothed over three samples (clip keys are 30 Hz, the track 60 Hz)
	TArray<float> Raw;
	Raw.SetNumZeroed(Num);
	for (int32 i = 1; i < Num; ++i)
	{
		Raw[i] = FVector::Dist2D(RootPositions[i], RootPositions[i - 1]) * SampleRate;
	}
	Raw[0] = Raw[1];
	Speed.SetNumZeroed(Num);
	for (int32 i = 0; i < Num; ++i)
	{
		Speed[i] = (Raw[FMath::Max(i - 1, 0)] + Raw[i] + Raw[FMath::Min(i + 1, Num - 1)]) / 3.f;
	}
	// The stop: the first sample at rest after the clip's top speed
	int32 Peak = 0;
	for (int32 i = 0; i < Num; ++i)
	{
		if (Speed[i] > Speed[Peak])
		{
			Peak = i;
		}
	}
	StopIndex = Num - 1;
	for (int32 i = Peak; i < Num; ++i)
	{
		if (Speed[i] < 5.f)
		{
			StopIndex = i;
			break;
		}
	}
	// The run speed: the mean before the stop's last 0.5 s (the clips start in the run cycle)
	const int32 RunEnd = FMath::Max(StopIndex - FMath::RoundToInt(0.5f * SampleRate), 1);
	float Sum = 0.f;
	for (int32 i = 0; i < RunEnd; ++i)
	{
		Sum += Speed[i];
	}
	RunSpeed = Sum / RunEnd;
	// The brake starts at the last sample still at 95% of it
	BrakeStart = 0;
	for (int32 i = StopIndex; i >= 0; --i)
	{
		if (Speed[i] >= 0.95f * RunSpeed)
		{
			BrakeStart = i;
			break;
		}
	}
}

float FGSTN_RunStopTrack::TimeForSpeedFraction(float SpeedFraction) const
{
	if (!IsValid())
	{
		return 0.f;
	}
	const float Target = FMath::Clamp(SpeedFraction, 0.f, 1.f) * RunSpeed;
	for (int32 i = BrakeStart + 1; i <= StopIndex; ++i)
	{
		if (Speed[i] <= Target)
		{
			// Between i - 1 (above) and i (at or below)
			const float Above = Speed[i - 1];
			const float Alpha = Above > Speed[i] ? FMath::Clamp((Above - Target) / (Above - Speed[i]), 0.f, 1.f) : 1.f;
			return (i - 1 + Alpha) / SampleRate;
		}
	}
	return StopTime();
}

void FGSTN_RunStopState::Step(const FGSTN_RunStopSettings& Settings, EGSTN_RunStopMode Mode, float DeltaSeconds, float Speed, float Stick, bool bBlocked,
	bool bLeftFootSwinging, const FGSTN_RunStopTrack* LeftTrack, const FGSTN_RunStopTrack* RightTrack)
{
	const bool bHeld = Stick >= Settings.StickDeadzone;
	// Anything but a plain grounded run, or the stick pushed again, ends the stop at once (the base locomotion takes over)
	if (bBlocked || bHeld || Mode == EGSTN_RunStopMode::Off)
	{
		Active = EGSTN_RunStopMode::Off;
		FramesReleased = bHeld && !bBlocked ? -1 : FramesReleased;
		if (bHeld && !bBlocked)
		{
			HeldSpeed = Speed;
		}
		else if (bBlocked)
		{
			// A skid or an air frame never turns into a stop later
			HeldSpeed = 0.f;
			FramesReleased = -1;
		}
		return;
	}

	if (Active == EGSTN_RunStopMode::Off)
	{
		if (FramesReleased < 0)
		{
			FramesReleased = 0;
		}
		else
		{
			++FramesReleased;
		}
		if (FramesReleased < Settings.ConfirmFrames || HeldSpeed < Settings.MinEntrySpeed)
		{
			return;
		}
		const FGSTN_RunStopTrack* Track = bLeftFootSwinging ? LeftTrack : RightTrack;
		if (Mode == EGSTN_RunStopMode::Clip && !(Track && Track->IsValid()))
		{
			return;
		}
		// Starts once per release
		Active = Mode;
		bLeftFoot = bLeftFootSwinging;
		EntrySpeed = HeldSpeed;
		HeldSpeed = 0.f;
		Elapsed = 0.f;
		ClipTime = Track && Track->IsValid() ? Track->BrakeStartTime() : 0.f;
	}
	else
	{
		Elapsed += DeltaSeconds;
	}

	if (Active == EGSTN_RunStopMode::Blend)
	{
		ClipTime = Elapsed;
		if (Elapsed >= Settings.BlendSeconds && Speed <= Settings.StoppedSpeed)
		{
			Active = EGSTN_RunStopMode::Off;
		}
		return;
	}

	const FGSTN_RunStopTrack* Track = bLeftFoot ? LeftTrack : RightTrack;
	if (!(Track && Track->IsValid()))
	{
		Active = EGSTN_RunStopMode::Off;
		return;
	}
	if (Speed > Settings.StoppedSpeed && ClipTime < Track->StopTime() && Elapsed < Settings.MaxBrakeSeconds)
	{
		// Braking: the clip time follows the sim's speed, never backwards
		const float Fraction = EntrySpeed > KINDA_SMALL_NUMBER ? Speed / EntrySpeed : 0.f;
		ClipTime = FMath::Max(ClipTime, Track->TimeForSpeedFraction(Fraction));
	}
	else
	{
		// Stopped: the settle plays on at its own rate
		ClipTime = FMath::Max(ClipTime, Track->StopTime()) + DeltaSeconds * Settings.SettleRate;
	}
	if (ClipTime >= Track->StopTime() + Settings.SettleClipSeconds)
	{
		Active = EGSTN_RunStopMode::Off;
	}
}

void FGSTN_FastCrouchState::Step(bool bEnabled, bool bCrouched, bool bGrounded, float Speed, float DeltaTime, float EnterLength, float ExitLength,
	float MoveSpeed)
{
	const bool bStill = Speed <= MoveSpeed;
	// The crouch stance comes a frame or two after the landing (combat tick): count a few frames as "just landed"
	TimeSinceLand = !bGrounded ? 0.f : (!bWasGrounded ? 0.f : TimeSinceLand + DeltaTime);
	const bool bLanded = bGrounded && TimeSinceLand <= 4.f / 60.f;
	if (!bEnabled || !bGrounded)
	{
		Phase = EGSTN_FastCrouchPhase::Off;
	}
	else if (bCrouched && bStill)
	{
		if (Phase == EGSTN_FastCrouchPhase::Off || Phase == EGSTN_FastCrouchPhase::Exit)
		{
			// Landing crouched (air crouch held): the crouch pose at once, near a hard cut (brief: no standing land in between).
			// A crouch walk that stops: the crouch pose too, it is already down
			const bool bAlreadyDown = bLanded || (bWasCrouched && Phase == EGSTN_FastCrouchPhase::Off);
			Phase = bAlreadyDown ? EGSTN_FastCrouchPhase::Idle : EGSTN_FastCrouchPhase::Enter;
			Time = 0.f;
		}
		else
		{
			Time += DeltaTime;
			if (Phase == EGSTN_FastCrouchPhase::Enter && Time >= EnterLength)
			{
				Phase = EGSTN_FastCrouchPhase::Idle;
				Time = 0.f;
			}
		}
	}
	else if (!bCrouched && bStill && (Phase == EGSTN_FastCrouchPhase::Enter || Phase == EGSTN_FastCrouchPhase::Idle))
	{
		Phase = EGSTN_FastCrouchPhase::Exit;
		Time = 0.f;
	}
	else if (Phase == EGSTN_FastCrouchPhase::Exit && bStill)
	{
		Time += DeltaTime;
		if (Time >= ExitLength)
		{
			Phase = EGSTN_FastCrouchPhase::Off;
			Time = 0.f;
		}
	}
	else
	{
		// Moving: GASP's crouch walk / run
		Phase = EGSTN_FastCrouchPhase::Off;
		Time = 0.f;
	}
	bWasCrouched = bCrouched;
	bWasGrounded = bGrounded;
}
