// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Net/GSTN_MinionNetRelevancy.h"

#include "Algo/BinarySearch.h"
#include "Misc/Crc.h"

const FGSTN_MinionPackGrant* FGSTN_MinionRelevancyState::Find(int32 PackId) const
{
	const int32 Index = Algo::LowerBoundBy(Grants, PackId, &FGSTN_MinionPackGrant::PackId);
	return Grants.IsValidIndex(Index) && Grants[Index].PackId == PackId ? &Grants[Index] : nullptr;
}

bool FGSTN_MinionRelevancyState::IsGranted(int32 PackId) const
{
	const FGSTN_MinionPackGrant* Grant = Find(PackId);
	return Grant && Grant->bGranted;
}

bool FGSTN_MinionRelevancyMath::RanksBefore(const FGSTN_MinionPackCandidate& A, const FGSTN_MinionPackCandidate& B)
{
	if (A.bAttacking != B.bAttacking)
	{
		return A.bAttacking;
	}
	if (A.bRecentlyHit != B.bRecentlyHit)
	{
		return A.bRecentlyHit;
	}
	if (A.NearestDistance != B.NearestDistance)
	{
		return A.NearestDistance < B.NearestDistance;
	}
	return A.PackId < B.PackId;
}

FGSTN_MinionRelevancyResult FGSTN_MinionRelevancyMath::Evaluate(const FGSTN_MinionRelevancyParams& Params, TArray<FGSTN_MinionPackCandidate>& Candidates,
	int32 Frame, FGSTN_MinionRelevancyState& State)
{
	Candidates.Sort([](const FGSTN_MinionPackCandidate& A, const FGSTN_MinionPackCandidate& B) { return RanksBefore(A, B); });

	FGSTN_MinionRelevancyResult Result;
	TArray<FGSTN_MinionPackGrant, TInlineAllocator<16>> Next;
	Next.SetNum(Candidates.Num());
	TArray<bool, TInlineAllocator<16>> Locked;
	Locked.SetNum(Candidates.Num());

	// Pass 1: the packs the hysteresis holds keep their state, and a held grant is paid for first
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FGSTN_MinionPackCandidate& Candidate = Candidates[Index];
		FGSTN_MinionPackGrant& Grant = Next[Index];
		Grant.PackId = Candidate.PackId;
		if (const FGSTN_MinionPackGrant* Previous = State.Find(Candidate.PackId))
		{
			Grant = *Previous;
		}
		Locked[Index] = Grant.LastToggleFrame != INDEX_NONE && Frame - Grant.LastToggleFrame < Params.MinToggleFrames;
		if (Locked[Index] && Grant.bGranted)
		{
			Result.GrantedItems += Candidate.ItemCount;
			Result.GrantedBytesPerSecond += Candidate.PlannedBytesPerSecond;
		}
	}

	// Pass 2: the free packs in rank order
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FGSTN_MinionPackCandidate& Candidate = Candidates[Index];
		FGSTN_MinionPackGrant& Grant = Next[Index];
		const bool bInRange = Candidate.NearestDistance <= Params.RelevantDistance;
		if (Locked[Index])
		{
			const bool bWanted = Grant.bGranted ? bInRange : false;
			Result.HeldByHysteresis += (Grant.bGranted && !bWanted) ? 1 : 0;
			Result.Granted += Grant.bGranted ? 1 : 0;
			Result.OutOfRange += (!Grant.bGranted && !bInRange) ? 1 : 0;
			continue;
		}

		const bool bFits = Result.GrantedItems + Candidate.ItemCount <= Params.MaxItems
			&& Result.GrantedBytesPerSecond + Candidate.PlannedBytesPerSecond <= Params.BudgetBytesPerSecond;
		const bool bWanted = bInRange && bFits;
		if (bWanted)
		{
			Result.GrantedItems += Candidate.ItemCount;
			Result.GrantedBytesPerSecond += Candidate.PlannedBytesPerSecond;
			++Result.Granted;
		}
		else if (bInRange)
		{
			++Result.DeniedByBudget;
		}
		else
		{
			++Result.OutOfRange;
		}
		if (bWanted != Grant.bGranted || Grant.LastToggleFrame == INDEX_NONE)
		{
			// The first decision counts as a toggle too: a pack seen for the first time keeps its state for a full second
			Grant.bGranted = bWanted;
			Grant.LastToggleFrame = Frame;
		}
	}

	State.Grants.Reset(Next.Num());
	State.Grants.Append(Next);
	State.Grants.Sort([](const FGSTN_MinionPackGrant& A, const FGSTN_MinionPackGrant& B) { return A.PackId < B.PackId; });
	State.LastEvaluationFrame = Frame;
	return Result;
}

uint32 FGSTN_MinionRegistryHash::Compute(TConstArrayView<FString> PathsByTypeIndex)
{
	// The count first, so a registry that is a prefix of another never hashes the same
	const int32 Count = PathsByTypeIndex.Num();
	uint32 Hash = FCrc::MemCrc32(&Count, sizeof(Count), 0x4D494E49u);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Hash = FCrc::MemCrc32(&Index, sizeof(Index), Hash);
		const FString& Path = PathsByTypeIndex[Index];
		// Lower case: object paths compare case-insensitively in the engine, so a case difference is not a different registry
		const FString Lower = Path.ToLower();
		Hash = FCrc::MemCrc32(*Lower, Lower.Len() * sizeof(TCHAR), Hash);
	}
	return Hash;
}
