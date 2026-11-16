// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "LOD/GSTN_MinionLOD.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

float FGSTN_MinionLODParams::GetBorder(EGSTN_MinionLODTier Tier) const
{
	switch (Tier)
	{
	case EGSTN_MinionLODTier::Combat: return CombatDistance;
	case EGSTN_MinionLODTier::Near: return NearDistance;
	case EGSTN_MinionLODTier::Far: return FarDistance;
	default: return TNumericLimits<float>::Max();
	}
}

int32 FGSTN_MinionLODParams::GetInterval(EGSTN_MinionLODTier Tier) const
{
	switch (Tier)
	{
	case EGSTN_MinionLODTier::Combat: return FMath::Max(CombatInterval, 1);
	case EGSTN_MinionLODTier::Near: return FMath::Max(NearInterval, 1);
	case EGSTN_MinionLODTier::Far: return FMath::Max(FarInterval, 1);
	default: return FMath::Max(DormantInterval, 1);
	}
}

bool FGSTN_MinionLODParams::IsConsistent() const
{
	return CombatDistance > 0.f && NearDistance > CombatDistance && FarDistance > NearDistance
		&& HysteresisFraction >= 0.f && HysteresisFraction < 0.5f
		&& MinDwellFrames >= 0 && MaxActiveCombat >= 1
		&& CombatInterval >= 1 && NearInterval >= 1 && FarInterval >= 1 && DormantInterval >= 1;
}

FPrimaryAssetId UGSTN_MinionLODSettings::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("GSTN_MinionLODSettings")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_MinionLODSettings::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!Params.IsConsistent())
	{
		Context.AddError(FText::FromString(TEXT("The LOD borders must strictly increase (Combat < Near < Far), the hysteresis must be below 0.5 and every interval at least 1.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

EGSTN_MinionLODTier FGSTN_MinionLODMath::TierForDistance(const FGSTN_MinionLODParams& Params, float Distance)
{
	if (!(Distance >= 0.f))
	{
		return EGSTN_MinionLODTier::Combat;
	}
	if (Distance <= Params.CombatDistance)
	{
		return EGSTN_MinionLODTier::Combat;
	}
	if (Distance <= Params.NearDistance)
	{
		return EGSTN_MinionLODTier::Near;
	}
	if (Distance <= Params.FarDistance)
	{
		return EGSTN_MinionLODTier::Far;
	}
	return EGSTN_MinionLODTier::Dormant;
}

EGSTN_MinionLODTier FGSTN_MinionLODMath::Evaluate(const FGSTN_MinionLODParams& Params, EGSTN_MinionLODTier Current, float Distance, int32 FramesInTier, bool bForceCombat)
{
	if (bForceCombat || !(Distance >= 0.f))
	{
		return EGSTN_MinionLODTier::Combat;
	}
	if (FramesInTier < Params.MinDwellFrames)
	{
		return Current;
	}

	const float Outward = 1.f + Params.HysteresisFraction;
	const float Inward = 1.f - Params.HysteresisFraction;
	uint8 Tier = static_cast<uint8>(Current);
	const uint8 Last = static_cast<uint8>(EGSTN_MinionLODTier::Dormant);

	// Outward first: a minion that crossed several borders at once (teleport, fast fighter) lands in the right tier in one step
	while (Tier < Last && Distance > Params.GetBorder(static_cast<EGSTN_MinionLODTier>(Tier)) * Outward)
	{
		++Tier;
	}
	if (Tier != static_cast<uint8>(Current))
	{
		return static_cast<EGSTN_MinionLODTier>(Tier);
	}

	while (Tier > 0 && Distance < Params.GetBorder(static_cast<EGSTN_MinionLODTier>(Tier - 1)) * Inward)
	{
		--Tier;
	}
	return static_cast<EGSTN_MinionLODTier>(Tier);
}

bool FGSTN_MinionLODMath::Step(const FGSTN_MinionLODParams& Params, FGSTN_MinionLODState& State, float Distance, bool bForceCombat, int32 DeltaFrames)
{
	// Saturate instead of overflowing on a minion parked in one tier for a very long time
	State.FramesInTier = (State.FramesInTier > MAX_int32 - DeltaFrames) ? MAX_int32 : State.FramesInTier + FMath::Max(DeltaFrames, 0);

	const EGSTN_MinionLODTier Next = Evaluate(Params, State.Tier, Distance, State.FramesInTier, bForceCombat);
	if (Next == State.Tier)
	{
		return false;
	}
	State.Tier = Next;
	State.FramesInTier = 0;
	return true;
}

int32 GSTNMinionLODCap::SelectFarthest(TArrayView<FGSTN_MinionCapCandidate> Candidates, int32 Count)
{
	const int32 Num = Candidates.Num();
	const int32 Take = FMath::Clamp(Count, 0, Num);
	for (int32 Pick = 0; Pick < Take; ++Pick)
	{
		int32 Best = Pick;
		for (int32 Index = Pick + 1; Index < Num; ++Index)
		{
			if (IsFartherThan(Candidates[Index], Candidates[Best]))
			{
				Best = Index;
			}
		}
		if (Best != Pick)
		{
			Swap(Candidates[Pick], Candidates[Best]);
		}
	}
	return Take;
}
