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
	float DeltaSeconds, float StateScale)
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

	const FVector Target = Settings.bEnabled ? TargetLean(Settings, Horizontal, FilteredAccel, Forward, StateScale) : FVector::ZeroVector;

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
	Lean = Lean.GetClampedToMaxSize(Settings.MaxAngleDeg * 1.2f);
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
