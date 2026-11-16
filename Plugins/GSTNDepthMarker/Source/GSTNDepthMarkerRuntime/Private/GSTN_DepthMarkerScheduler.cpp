// Depth marker scheduling (depth-marker.md §3). Pure.

#include "GSTN_DepthMarkerScheduler.h"

void FGSTN_DepthMarkerScheduler::Rank(TArray<FGSTN_DepthRankEntry>& Entries, int32 MaxMarkers, TArray<int32>& OutDropped)
{
	OutDropped.Reset();
	Entries.Sort([](const FGSTN_DepthRankEntry& A, const FGSTN_DepthRankEntry& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority < B.Priority;
		}
		if (A.bRecentlyHit != B.bRecentlyHit)
		{
			return A.bRecentlyHit;
		}
		if (!FMath::IsNearlyEqual(A.DistanceSq, B.DistanceSq))
		{
			return A.DistanceSq < B.DistanceSq;
		}
		return A.Id < B.Id;
	});

	const int32 Keep = FMath::Max(MaxMarkers, 0);
	for (int32 Index = Keep; Index < Entries.Num(); ++Index)
	{
		OutDropped.Add(Entries[Index].Id);
	}
	if (Entries.Num() > Keep)
	{
		Entries.SetNum(Keep);
	}
}

double FGSTN_DepthMarkerScheduler::RankInterval(int32 Rank, const FGSTN_DepthSchedulerConfig& Config)
{
	if (Rank < Config.FullRateRanks)
	{
		return 0.0;
	}
	if (Rank < Config.MidRateRanks)
	{
		return 1.0 / FMath::Max(Config.MidRateHz, 1.f);
	}
	return 1.0 / FMath::Max(Config.LowRateHz, 1.f);
}

void FGSTN_DepthMarkerScheduler::PlanProbes(const TArray<FGSTN_DepthRankEntry>& Ranked, const FGSTN_DepthSchedulerConfig& Config, double Time,
	TArray<int32>& OutIds)
{
	struct FDue
	{
		int32 Id;
		int32 Rank;
		double LastProbeTime;
	};
	TArray<FDue> Due;
	for (int32 Rank = 0; Rank < Ranked.Num(); ++Rank)
	{
		const FGSTN_DepthRankEntry& E = Ranked[Rank];
		if (E.bWantsProbe && (Time - E.LastProbeTime) >= RankInterval(Rank, Config))
		{
			Due.Add({ E.Id, Rank, E.LastProbeTime });
		}
	}
	// Oldest first, ties to the better rank: the cap never starves a marker that has waited longest
	Due.Sort([](const FDue& A, const FDue& B)
	{
		if (A.LastProbeTime != B.LastProbeTime)
		{
			return A.LastProbeTime < B.LastProbeTime;
		}
		return A.Rank < B.Rank;
	});

	OutIds.Reset();
	const int32 Count = FMath::Min(Due.Num(), FMath::Max(Config.MaxTraces, 0));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutIds.Add(Due[Index].Id);
	}
}

bool FGSTN_DepthMarkerScheduler::WantsProbe(const FGSTN_DepthSubjectState& Subject, bool bMarkerVisible)
{
	if (Subject.bSuppressed)
	{
		return false;
	}
	// Grounded and nothing showing: the common case of every enemy costs nothing
	return Subject.bAirborne || !Subject.bGrounded || bMarkerVisible;
}
