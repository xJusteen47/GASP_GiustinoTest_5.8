// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Present/GSTN_MinionInterpolator.h"

#include "Algo/Sort.h"
#include "Net/GSTN_MinionNetCodec.h"

namespace
{
	/** Interpolated, extrapolated and held poses are one family (one clock); a switch between families is blended */
	int32 ModeFamily(EGSTN_MinionDisplayMode Mode)
	{
		switch (Mode)
		{
		case EGSTN_MinionDisplayMode::Interpolated:
		case EGSTN_MinionDisplayMode::Extrapolated:
		case EGSTN_MinionDisplayMode::Held:
			return 1;
		case EGSTN_MinionDisplayMode::DeadReckoned:
			return 2;
		case EGSTN_MinionDisplayMode::Direct:
			return 3;
		default:
			return 0;
		}
	}

	float WrapYaw(float Degrees)
	{
		float Wrapped = FMath::Fmod(Degrees, 360.f);
		return Wrapped < 0.f ? Wrapped + 360.f : Wrapped;
	}

	FGSTN_MinionDisplayPose PoseOf(const FGSTN_MinionPresentSample& Sample, EGSTN_MinionDisplayMode Mode)
	{
		FGSTN_MinionDisplayPose Pose;
		Pose.Position = Sample.Position;
		Pose.YawDegrees = Sample.YawDegrees;
		Pose.Frame = Sample.Frame;
		Pose.ActionState = Sample.ActionState;
		Pose.MoveId = Sample.MoveId;
		Pose.MoveStartFrame = Sample.MoveStartFrame;
		Pose.Speed = Sample.Speed;
		Pose.Mode = Mode;
		return Pose;
	}
}

void FGSTN_MinionInterpolator::Reset()
{
	*this = FGSTN_MinionInterpolator();
}

bool FGSTN_MinionInterpolator::AddSample(const FGSTN_MinionPresentSample& Sample, double NowSeconds)
{
	const bool bTeleport = (Sample.Flags & GSTNMinion::NetFlags::Teleport) != 0;
	if (bTeleport)
	{
		// Spawn, respawn, teleport: nothing before this sample is related to it
		Head = 0;
		Count = 0;
		bSnapNext = true;
		bPendingCorrection = false;
		CorrectionStartSeconds = -1.0;
	}
	else if (Count > 0 && Sample.Frame <= GetNewest()->Frame)
	{
		return false;
	}

	if (Count < RingSize)
	{
		Ring[(Head + Count) % RingSize] = Sample;
		++Count;
	}
	else
	{
		Ring[Head] = Sample;
		Head = (Head + 1) % RingSize;
	}
	NewestArrivalSeconds = NowSeconds;
	// Dead reckoning jumps to the new target: the next pose keeps the old one and blends the difference out
	if (!bTeleport && LastPose.Mode == EGSTN_MinionDisplayMode::DeadReckoned)
	{
		bPendingCorrection = true;
	}
	return true;
}

FGSTN_MinionDisplayPose FGSTN_MinionInterpolator::Interpolate(double RenderFrame, double NowSeconds, const FGSTN_MinionSmoothingParams& Params)
{
	if (Count == 0)
	{
		return FGSTN_MinionDisplayPose();
	}
	const FGSTN_MinionPresentSample& Oldest = At(0);
	const FGSTN_MinionPresentSample& Newest = At(Count - 1);
	const double StepSeconds = FMath::Max(Params.FixedStepMs, 1.f) / 1000.0;

	FGSTN_MinionDisplayPose Raw;
	if (RenderFrame < Oldest.Frame)
	{
		// Render time older than the ring (a sample burst after a stall): show the oldest one we have
		Raw = PoseOf(Oldest, EGSTN_MinionDisplayMode::Held);
	}
	else if (RenderFrame >= Newest.Frame)
	{
		const double CapFrames = Params.ExtrapolationCapMs / FMath::Max(Params.FixedStepMs, 1.f);
		const double Ahead = RenderFrame - Newest.Frame;
		if (Ahead <= CapFrames)
		{
			Raw = PoseOf(Newest, Ahead > 0.0 ? EGSTN_MinionDisplayMode::Extrapolated : EGSTN_MinionDisplayMode::Interpolated);
			Raw.Position = Advance(Newest, Ahead * StepSeconds);
			Raw.Frame = RenderFrame;
		}
		else
		{
			// Past the cap the puppet stops where the extrapolation ended, not back at the sample (no backwards pop)
			Raw = PoseOf(Newest, EGSTN_MinionDisplayMode::Held);
			Raw.Position = Advance(Newest, CapFrames * StepSeconds);
			Raw.Frame = Newest.Frame + CapFrames;
		}
	}
	else
	{
		int32 Index = 0;
		while (Index + 2 < Count && At(Index + 1).Frame <= RenderFrame)
		{
			++Index;
		}
		const FGSTN_MinionPresentSample& From = At(Index);
		const FGSTN_MinionPresentSample& To = At(Index + 1);
		const float Alpha = static_cast<float>(FMath::Clamp((RenderFrame - From.Frame) / FMath::Max(To.Frame - From.Frame, 1), 0.0, 1.0));
		Raw = PoseOf(From, EGSTN_MinionDisplayMode::Interpolated);
		Raw.Position = FMath::Lerp(From.Position, To.Position, Alpha);
		Raw.YawDegrees = LerpYaw(From.YawDegrees, To.YawDegrees, Alpha);
		Raw.Speed = FMath::Lerp(From.Speed, To.Speed, Alpha);
		Raw.Frame = RenderFrame;
	}
	return Finish(Raw, NowSeconds, Params);
}

FGSTN_MinionDisplayPose FGSTN_MinionInterpolator::DeadReckon(double NowSeconds, const FGSTN_MinionSmoothingParams& Params)
{
	if (Count == 0)
	{
		return FGSTN_MinionDisplayPose();
	}
	const FGSTN_MinionPresentSample& Newest = At(Count - 1);
	// The horizon (header, FarDeadReckoningCapMs): the cap, the sender's own interval, and the far cap for senders slower than T1
	double CapSeconds = Params.DeadReckoningCapMs / 1000.0;
	if (Count >= 2)
	{
		const int32 SampleGap = Newest.Frame - At(Count - 2).Frame;
		if (SampleGap > 0)
		{
			CapSeconds = FMath::Min(CapSeconds, SampleGap * FMath::Max(Params.FixedStepMs, 1.f) / 1000.0);
		}
		if (SampleGap > Params.T1MaxFrameGap)
		{
			CapSeconds = FMath::Min(CapSeconds, Params.FarDeadReckoningCapMs / 1000.0);
		}
	}
	const double Elapsed = FMath::Clamp(NowSeconds - NewestArrivalSeconds, 0.0, CapSeconds);
	FGSTN_MinionDisplayPose Raw = PoseOf(Newest, EGSTN_MinionDisplayMode::DeadReckoned);
	Raw.Position = Advance(Newest, Elapsed);
	Raw.Frame = Newest.Frame + Elapsed * 1000.0 / FMath::Max(Params.FixedStepMs, 1.f);

	if (bPendingCorrection)
	{
		bPendingCorrection = false;
		if (!bSnapNext && LastPose.Mode != EGSTN_MinionDisplayMode::None)
		{
			StartCorrection(Raw, NowSeconds);
		}
	}
	return Finish(Raw, NowSeconds, Params);
}

FGSTN_MinionDisplayPose FGSTN_MinionInterpolator::Evaluate(double RenderFrame, double NowSeconds, const FGSTN_MinionSmoothingParams& Params)
{
	return GetInferredTier(Params, NowSeconds) == EGSTN_MinionLODTier::Combat ? Interpolate(RenderFrame, NowSeconds, Params) : DeadReckon(NowSeconds, Params);
}

EGSTN_MinionLODTier FGSTN_MinionInterpolator::GetInferredTier(const FGSTN_MinionSmoothingParams& Params, double NowSeconds) const
{
	if (Count < 2)
	{
		return EGSTN_MinionLODTier::Far;
	}
	int32 Gap = At(Count - 1).Frame - At(Count - 2).Frame;
	if (NowSeconds >= 0.0)
	{
		const double SilentFrames = (NowSeconds - NewestArrivalSeconds) * 1000.0 / FMath::Max(Params.FixedStepMs, 1.f);
		Gap = FMath::Max(Gap, FMath::FloorToInt32(SilentFrames));
	}
	if (Gap <= Params.T0MaxFrameGap)
	{
		return EGSTN_MinionLODTier::Combat;
	}
	return Gap <= Params.T1MaxFrameGap ? EGSTN_MinionLODTier::Near : EGSTN_MinionLODTier::Far;
}

FVector3f FGSTN_MinionInterpolator::Advance(const FGSTN_MinionPresentSample& Sample, double Seconds)
{
	if (Seconds <= 0.0 || Sample.Speed <= 0.f)
	{
		return Sample.Position;
	}
	float Sin = 0.f;
	float Cos = 1.f;
	FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(Sample.YawDegrees));
	return Sample.Position + FVector3f(Cos, Sin, 0.f) * (Sample.Speed * static_cast<float>(Seconds));
}

float FGSTN_MinionInterpolator::LerpYaw(float From, float To, float Alpha)
{
	return WrapYaw(From + FMath::FindDeltaAngleDegrees(From, To) * Alpha);
}

void FGSTN_MinionInterpolator::StartCorrection(const FGSTN_MinionDisplayPose& Target, double NowSeconds)
{
	CorrectionOffset = LastPose.Position - Target.Position;
	CorrectionYaw = FMath::FindDeltaAngleDegrees(Target.YawDegrees, LastPose.YawDegrees);
	CorrectionStartSeconds = NowSeconds;
}

FGSTN_MinionDisplayPose FGSTN_MinionInterpolator::Finish(FGSTN_MinionDisplayPose Raw, double NowSeconds, const FGSTN_MinionSmoothingParams& Params)
{
	if (bSnapNext)
	{
		bSnapNext = false;
		CorrectionStartSeconds = -1.0;
	}
	else if (LastPose.Mode != EGSTN_MinionDisplayMode::None && ModeFamily(LastPose.Mode) != ModeFamily(Raw.Mode))
	{
		StartCorrection(Raw, NowSeconds);
	}

	FGSTN_MinionDisplayPose Pose = Raw;
	if (CorrectionStartSeconds >= 0.0)
	{
		const double BlendSeconds = Params.CorrectionBlendMs / 1000.0;
		const double Alpha = BlendSeconds > 0.0 ? (NowSeconds - CorrectionStartSeconds) / BlendSeconds : 1.0;
		// The tolerance absorbs the rounding of local times (1.2 - 1.0 is a hair under 0.2): a finished blend reports finished
		if (Alpha >= 1.0 - UE_KINDA_SMALL_NUMBER)
		{
			CorrectionStartSeconds = -1.0;
		}
		else
		{
			const float Remaining = static_cast<float>(1.0 - FMath::Max(Alpha, 0.0));
			Pose.Position += CorrectionOffset * Remaining;
			Pose.YawDegrees = WrapYaw(Raw.YawDegrees + CorrectionYaw * Remaining);
			Pose.bBlending = true;
		}
	}
	LastPose = Pose;
	return Pose;
}

// --- Hit flash ---

void FGSTN_MinionHitFlash::ObserveCounter(uint8 Counter, double NowSeconds, const FGSTN_MinionFlashParams& Params)
{
	Counter &= 0x07;
	if (!bHasCounter)
	{
		bHasCounter = true;
		LastCounter = Counter;
		return;
	}
	if (Counter == LastCounter)
	{
		return;
	}
	Tick(NowSeconds);
	int32 Delta = (Counter - LastCounter) & 0x07;
	LastCounter = Counter;
	if (Pending > 0)
	{
		const int32 Confirmed = FMath::Min(Delta, Pending);
		Pending -= Confirmed;
		NumConfirmed += Confirmed;
		Delta -= Confirmed;
		// The effect the prediction started goes on: a confirm never restarts the flash (no double flash)
		State = EGSTN_MinionFlashState::Confirmed;
	}
	if (Delta > 0)
	{
		NumRemote += Delta;
		State = EGSTN_MinionFlashState::Remote;
		StartEffect(NowSeconds, Params);
	}
}

void FGSTN_MinionHitFlash::OnPredictedHit(double NowSeconds, const FGSTN_MinionFlashParams& Params)
{
	Tick(NowSeconds);
	++Pending;
	++NumPredicted;
	ConfirmDeadline = NowSeconds + Params.ConfirmWindowSeconds;
	State = EGSTN_MinionFlashState::Predicted;
	StartEffect(NowSeconds, Params);
}

void FGSTN_MinionHitFlash::OnPredictionCancelled(double NowSeconds)
{
	if (Pending <= 0)
	{
		return;
	}
	--Pending;
	++NumCancelled;
	if (Pending == 0)
	{
		State = EGSTN_MinionFlashState::Cancelled;
		EndEffect();
	}
}

void FGSTN_MinionHitFlash::Tick(double NowSeconds)
{
	if (Pending > 0 && NowSeconds > ConfirmDeadline)
	{
		NumExpired += Pending;
		Pending = 0;
		State = EGSTN_MinionFlashState::Expired;
		EndEffect();
	}
}

float FGSTN_MinionHitFlash::GetFlash(double NowSeconds, const FGSTN_MinionFlashParams& Params) const
{
	if (FlashStart < 0.0 || Params.FlashSeconds <= 0.f)
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Clamp(1.0 - (NowSeconds - FlashStart) / Params.FlashSeconds, 0.0, 1.0));
}

bool FGSTN_MinionHitFlash::IsPaused(double NowSeconds) const
{
	return PauseEnd >= 0.0 && NowSeconds < PauseEnd;
}

const TCHAR* FGSTN_MinionHitFlash::StateName(EGSTN_MinionFlashState Value)
{
	switch (Value)
	{
	case EGSTN_MinionFlashState::Predicted: return TEXT("predicted");
	case EGSTN_MinionFlashState::Confirmed: return TEXT("confirmed");
	case EGSTN_MinionFlashState::Remote: return TEXT("remote");
	case EGSTN_MinionFlashState::Cancelled: return TEXT("cancelled");
	case EGSTN_MinionFlashState::Expired: return TEXT("expired");
	default: return TEXT("none");
	}
}

void FGSTN_MinionHitFlash::StartEffect(double NowSeconds, const FGSTN_MinionFlashParams& Params)
{
	FlashStart = NowSeconds;
	PauseEnd = NowSeconds + Params.PauseSeconds;
}

void FGSTN_MinionHitFlash::EndEffect()
{
	FlashStart = -1.0;
	PauseEnd = -1.0;
}

// --- Visible cut ---

void GSTNMinionPresent::SelectNearest(TConstArrayView<float> DistancesSq, TConstArrayView<bool> bHasPuppet, int32 MaxVisible, float StickyFraction,
	TArray<int32>& OutVisible)
{
	OutVisible.Reset();
	const int32 Num = DistancesSq.Num();
	if (MaxVisible <= 0 || Num == 0)
	{
		return;
	}
	const float Keep = FMath::Square(FMath::Clamp(1.f - StickyFraction, 0.f, 1.f));
	TArray<TPair<float, int32>, TInlineAllocator<256>> Keys;
	Keys.Reserve(Num);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const bool bSticky = bHasPuppet.IsValidIndex(Index) && bHasPuppet[Index];
		Keys.Emplace(bSticky ? DistancesSq[Index] * Keep : DistancesSq[Index], Index);
	}
	Algo::Sort(Keys, [](const TPair<float, int32>& A, const TPair<float, int32>& B)
	{
		return A.Key < B.Key || (A.Key == B.Key && A.Value < B.Value);
	});
	const int32 Take = FMath::Min(MaxVisible, Num);
	OutVisible.Reserve(Take);
	for (int32 Index = 0; Index < Take; ++Index)
	{
		OutVisible.Add(Keys[Index].Value);
	}
}
