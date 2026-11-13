// Depth marker scheduling: who is drawn and who probes this frame (depth-marker.md §3). Pure, no world.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_DepthMarkerTypes.h"

/** One registered marker as the scheduler sees it */
struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthRankEntry
{
	int32 Id = INDEX_NONE;

	/** 0 local player, 1 ally, 2 everyone else: lower ranks first */
	int32 Priority = 2;

	float DistanceSq = 0.f;

	/** Recently hit enemies rank before quiet ones of the same priority */
	bool bRecentlyHit = false;

	double LastProbeTime = -1.0e9;

	bool bWantsProbe = true;
};

struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthSchedulerConfig
{
	int32 MaxMarkers = 12;
	int32 MaxTraces = 8;
	int32 FullRateRanks = 4;
	int32 MidRateRanks = 8;
	float MidRateHz = 30.f;
	float LowRateHz = 10.f;
};

struct GSTNDEPTHMARKERRUNTIME_API FGSTN_DepthMarkerScheduler
{
	/**
	 * Sorts the entries by priority, then recently hit, then distance, then id (stable and deterministic), keeps the first
	 * MaxMarkers and returns the ids that did not make the cut in OutDropped.
	 */
	static void Rank(TArray<FGSTN_DepthRankEntry>& InOutEntries, int32 MaxMarkers, TArray<int32>& OutDropped);

	/** Seconds between two probes of a rank: 0 for the first ranks, then 1/MidRateHz, then 1/LowRateHz */
	static double RankInterval(int32 Rank, const FGSTN_DepthSchedulerConfig& Config);

	/**
	 * Which of the ranked entries probe now: those that want one and are due by their rank rate, oldest first, at most
	 * MaxTraces of them (ties go to the better rank). Local players are never starved by the cap because they rank first.
	 */
	static void PlanProbes(const TArray<FGSTN_DepthRankEntry>& Ranked, const FGSTN_DepthSchedulerConfig& Config, double Time,
		TArray<int32>& OutIds);

	/** Step 2 of the algorithm: a grounded subject whose marker is not showing does not probe at all (the common enemy case) */
	static bool WantsProbe(const FGSTN_DepthSubjectState& Subject, bool bMarkerVisible);
};
