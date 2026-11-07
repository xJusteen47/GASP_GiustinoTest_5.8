// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

/**
 * Budgeted relevancy of the packs (combat-m8-minions.md 8.1 "Budgeted relevancy"), the pure part: ranking, the grant walk under the
 * item and byte budgets, and the 1 s hysteresis. The Runtime module fills the candidates every ~100 ms per connection (distances from
 * the store, flags from the batch columns) and answers IsNetRelevantFor from the state this produces.
 *
 * Why packs are ranked rather than simply cut at 8000 cm: on a listen host the upload is the limit (8.3), and the packs that matter
 * to a player are the ones attacking it or just hit, not merely the nearest. A denied pack is not relevant: its actor channel closes
 * on that client and the client registry drops its items, which is the cost model of 8.3 (out of relevancy = 0 bytes).
 */

/** One pack as one connection sees it at the evaluation frame */
struct GSTNMINIONCORE_API FGSTN_MinionPackCandidate
{
	int32 PackId = INDEX_NONE;
	/** Members (items) of the pack */
	int32 ItemCount = 0;
	/** Sum over the members of rate x planning bytes (GSTNMinionNetRate::PlannedBytesPerSecond) */
	int32 PlannedBytesPerSecond = 0;
	/** Distance from the connection's viewer to the nearest member, cm */
	float NearestDistance = TNumericLimits<float>::Max();
	/** A member is running an attack (holds an attack token): the player must see who swings */
	bool bAttacking = false;
	/** A member took a hit in the last second */
	bool bRecentlyHit = false;
};

/** Numbers of the grant walk; the Runtime module fills them from its settings and cvars */
struct GSTNMINIONCORE_API FGSTN_MinionRelevancyParams
{
	/** MaxRelevantPerClient: items, not packs (8.1: the only reading that binds with 16-item packs and 128 slots) */
	int32 MaxItems = 64;
	/** This connection's share of gstn.Minion.HostUploadBudgetKBps, bytes per second */
	int32 BudgetBytesPerSecond = 40000;
	/** A pack is a candidate only while a member is within this distance of the viewer */
	float RelevantDistance = 8000.f;
	/** No pack toggles granted <-> denied faster than this (1 s at 62.5 Hz) */
	int32 MinToggleFrames = 63;
};

/** The grant of one pack for one connection and when it last changed */
struct GSTNMINIONCORE_API FGSTN_MinionPackGrant
{
	int32 PackId = INDEX_NONE;
	bool bGranted = false;
	/** Frame of the last toggle, INDEX_NONE = never evaluated (free to toggle) */
	int32 LastToggleFrame = INDEX_NONE;
};

/** Per-connection state between evaluations: the grants, sorted by pack id */
struct GSTNMINIONCORE_API FGSTN_MinionRelevancyState
{
	TArray<FGSTN_MinionPackGrant> Grants;
	/** Frame of the last evaluation, INDEX_NONE before the first (IsNetRelevantFor then falls back to the plain distance rule) */
	int32 LastEvaluationFrame = INDEX_NONE;

	bool HasEvaluated() const { return LastEvaluationFrame != INDEX_NONE; }
	bool IsGranted(int32 PackId) const;
	const FGSTN_MinionPackGrant* Find(int32 PackId) const;
};

/** What one evaluation did, for the stats and the tests */
struct GSTNMINIONCORE_API FGSTN_MinionRelevancyResult
{
	int32 Granted = 0;
	/** In range but over a budget */
	int32 DeniedByBudget = 0;
	int32 OutOfRange = 0;
	/** Packs whose wanted state differed from the current one but the hysteresis kept it */
	int32 HeldByHysteresis = 0;
	int32 GrantedItems = 0;
	int32 GrantedBytesPerSecond = 0;
};

struct GSTNMINIONCORE_API FGSTN_MinionRelevancyMath
{
	/**
	 * Priority order: attacking first, then recently hit, then nearest, then the lower pack id (a total order, so equal inputs give
	 * equal grants on every run)
	 */
	static bool RanksBefore(const FGSTN_MinionPackCandidate& A, const FGSTN_MinionPackCandidate& B);

	/**
	 * Ranks Candidates in place and updates State. Packs held by the hysteresis keep their state first and their cost counts against
	 * the budgets; the free ones are then granted in rank order while they are in range and fit both budgets (a pack that does not fit
	 * is skipped, a smaller one after it may still fit). A pack absent from Candidates is forgotten
	 */
	static FGSTN_MinionRelevancyResult Evaluate(const FGSTN_MinionRelevancyParams& Params, TArray<FGSTN_MinionPackCandidate>& Candidates,
		int32 Frame, FGSTN_MinionRelevancyState& State);
};

/**
 * The over-inclusion metric of 8.1: item updates sent to a connection for a member beyond RelevantDistance of it, over all item
 * updates sent. Above 30 % the spec falls back to re-packing by (cell, tier); the bench reads Ratio
 */
struct GSTNMINIONCORE_API FGSTN_MinionOverInclusion
{
	int64 ItemsSent = 0;
	int64 ItemsBeyond = 0;

	void Add(int32 Updates, bool bBeyond)
	{
		ItemsSent += Updates;
		ItemsBeyond += bBeyond ? Updates : 0;
	}

	double Ratio() const { return ItemsSent > 0 ? static_cast<double>(ItemsBeyond) / static_cast<double>(ItemsSent) : 0.0; }
};

/**
 * The registry hash checked at connect (8.2: the type index is an index into a list both sides must order the same way). The hash
 * covers (index, definition path) in index order; a path the client cannot resolve is hashed as an empty string, so a missing or
 * renamed definition on the client shows as a mismatch at the same index the server uses
 */
struct GSTNMINIONCORE_API FGSTN_MinionRegistryHash
{
	static uint32 Compute(TConstArrayView<FString> PathsByTypeIndex);
};
