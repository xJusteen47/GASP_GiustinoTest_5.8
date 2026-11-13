// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Net/GSTN_MinionNetRates.h"

#include "Net/GSTN_MinionNetCodec.h"

namespace GSTNMinionNetRate
{
	namespace
	{
		int32 CombatRateHz = DefaultCombatRateHz;
	}

	void SetCombatRateHz(int32 Hz)
	{
		CombatRateHz = FMath::Clamp(Hz, 5, MaxCombatRateHz);
	}

	int32 GetCombatRateHz()
	{
		return CombatRateHz;
	}

	int32 RateHz(EGSTN_MinionLODTier Tier)
	{
		switch (Tier)
		{
		case EGSTN_MinionLODTier::Combat:
			return CombatRateHz;
		case EGSTN_MinionLODTier::Near:
			return 5;
		case EGSTN_MinionLODTier::Far:
			return 1;
		default:
			return 0;
		}
	}

	int32 PeriodMilliFrames(int32 Rate)
	{
		if (Rate <= 0)
		{
			return MAX_int32;
		}
		// 1000 milli-frames per frame: period = 1000 x (62.5 / Rate) = 62500 / Rate, rounded up so the rate is a true ceiling
		return (FixedTickMilliHz + Rate - 1) / Rate;
	}

	int32 PackCeilingHz(TConstArrayView<uint8> MemberTiers)
	{
		int32 Best = 0;
		for (const uint8 Tier : MemberTiers)
		{
			Best = FMath::Max(Best, RateHz(static_cast<EGSTN_MinionLODTier>(Tier)));
		}
		return Best;
	}

	int32 PlannedBytesPerSecond(EGSTN_MinionLODTier Tier)
	{
		return RateHz(Tier) * PlanningBytesPerItem;
	}

	bool IsSignificantChange(const FGSTN_MinionNetWireItem& Sent, const FGSTN_MinionNetWireItem& Candidate)
	{
		// 1 cm steps: moved more than 3 cm means a squared step distance above 9
		int64 DistanceSquared = 0;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int64 Delta = static_cast<int64>(Candidate.PositionQ[Axis]) - static_cast<int64>(Sent.PositionQ[Axis]);
			DistanceSquared += Delta * Delta;
		}
		if (DistanceSquared > 9)
		{
			return true;
		}

		// 256 steps per turn: 4 degrees is 2.84 steps, so a wrapped difference of 3 steps or more is "more than 4 degrees"
		const int32 YawDelta = FMath::Abs(static_cast<int32>(Candidate.YawQ) - static_cast<int32>(Sent.YawQ));
		if (FMath::Min(YawDelta, 256 - YawDelta) >= 3)
		{
			return true;
		}

		return Sent.Id != Candidate.Id
			|| Sent.bHasType != Candidate.bHasType || Sent.TypeIndex != Candidate.TypeIndex
			|| Sent.bHasGlobalId != Candidate.bHasGlobalId || Sent.GlobalId != Candidate.GlobalId
			|| Sent.ActionState != Candidate.ActionState
			|| Sent.MoveId != Candidate.MoveId
			|| Sent.MoveStartLow != Candidate.MoveStartLow
			|| Sent.HitCounter != Candidate.HitCounter
			|| Sent.HealthQ != Candidate.HealthQ
			|| Sent.ExtByte != Candidate.ExtByte
			|| Sent.SpeedQ != Candidate.SpeedQ
			|| Sent.Flags != Candidate.Flags;
	}
}

bool FGSTN_MinionNetRateLimiter::CanSend(int32 Frame, int32 RateHz) const
{
	if (RateHz <= 0)
	{
		return false;
	}
	if (VirtualLastSend == MIN_int64)
	{
		return true;
	}
	const int64 Now = static_cast<int64>(Frame) * 1000;
	return Now + GSTNMinionNetRate::ToleranceMilliFrames >= VirtualLastSend + GSTNMinionNetRate::PeriodMilliFrames(RateHz);
}

void FGSTN_MinionNetRateLimiter::OnSent(int32 Frame, int32 RateHz)
{
	const int64 Now = static_cast<int64>(Frame) * 1000;
	if (VirtualLastSend == MIN_int64 || RateHz <= 0)
	{
		VirtualLastSend = Now;
		return;
	}
	// GCRA: a send on time advances the virtual clock by exactly one period (frame rounding does not drift the rate); a late send
	// restarts it at now, so being late never banks credit for a burst
	VirtualLastSend = FMath::Max(VirtualLastSend + GSTNMinionNetRate::PeriodMilliFrames(RateHz), Now);
}

int32 GSTNMinionNetRate::SelectPackSends(int32 Frame, FGSTN_MinionNetRateLimiter& PackLimiter, int32 CeilingHz, TConstArrayView<uint8> Tiers,
	TConstArrayView<bool> Dirty, TArrayView<FGSTN_MinionNetRateLimiter> ItemLimiters, TArray<int32>& OutSent)
{
	if (!PackLimiter.CanSend(Frame, CeilingHz))
	{
		return 0;
	}
	const int32 Num = FMath::Min3(Tiers.Num(), Dirty.Num(), ItemLimiters.Num());
	int32 Sent = 0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const int32 Rate = FMath::Min(RateHz(static_cast<EGSTN_MinionLODTier>(Tiers[Index])), CeilingHz);
		if (Dirty[Index] && ItemLimiters[Index].CanSend(Frame, Rate))
		{
			ItemLimiters[Index].OnSent(Frame, Rate);
			OutSent.Add(Index);
			++Sent;
		}
	}
	if (Sent > 0)
	{
		PackLimiter.OnSent(Frame, CeilingHz);
	}
	return Sent;
}
