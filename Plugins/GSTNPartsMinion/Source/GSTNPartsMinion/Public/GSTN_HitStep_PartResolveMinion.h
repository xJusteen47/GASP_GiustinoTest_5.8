// Bridge between parts and minions (plan P16).

#pragma once

#include "CoreMinimal.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "GSTN_HitStep_PartResolveMinion.generated.h"

/** Bench counters of the step (sub-gate B per hit cost): process-wide, game thread only, debug numbers and never read by the simulation */
struct FGSTN_PartsMinionStepTiming
{
	uint64 Cycles = 0;
	int32 Count = 0;
};

/**
 * The part resolve step as the minion victim pipeline runs it (combat-parts.md §5.5): the bridge puts this class in the minion
 * subsystem's extra victim step slot, which runs it right before ComputeKnockback, on a view whose snapshot extension the bridge built
 * from the minion's 9 byte part row. The part math is the parent's, unchanged (GSTNParts.Core.Minion.StepSameOutputAsFighter).
 *
 * Why a subclass and not the parent itself: the minion rules are usually the fighters' standard rules, which keep the two M4 steps
 * that PartResolve replaces in a parts-aware pipeline (ApplyDamage and HealthWeight, see UGSTN_HitStep_PartResolve). Run as an extra
 * step next to them, the parent would count the hit's damage twice and the health weight twice. This step makes the merged result
 * equal to the parts-aware pipeline's: it takes back the damage ApplyDamage will add (DamageAdd minus the hit's damage, so the
 * outcome ends at the part's MainDelta), drops the extra effects ApplyDamage appends again, and divides out the weight scale
 * HealthWeight already merged (the parent's weight already contains the damage progress).
 *
 * It does nothing (one pointer test) for a victim without a parts extension, i.e. every minion type without a part set, and nothing
 * when the rules already list PartResolve (that step ran earlier, on the same view). Pure and const like every step: the view in, the
 * result out.
 *
 * Cost (plan P16-C): the first version ran the parent's whole step and then HealthWeight again into a second full step result, and
 * scanned the rules' pipeline with class tests on every hit (4.7 us per part hit on the bench laptop). The step now keeps a small cache
 * per (part set, rules) pair, filled at the first hit and dropped by InvalidateStepCache (every registration of the bridge calls it): the
 * pipeline flags, the resolved penetration bands (the soft pointers are chased once) and the minimum weight scale. The hit itself runs
 * the same part math on that cache (ResolveCached), takes back ApplyDamage's share by arithmetic instead of running HealthWeight into a
 * scratch result, and writes no request: the minion batch never reads Requests (see ResolveCached). ResolveUncached is the first version,
 * kept as the reference the GSTNParts.Minion.CachedStepSameOutput test compares against, and as the fallback when the rules or the cache
 * switch (gstn.PartsMinion.StepCache 0) say so. Game thread only: the cache is process-wide static data, like the timing counters.
 */
UCLASS(meta = (DisplayName = "Part Resolve (Minion)"))
class GSTNPARTSMINION_API UGSTN_HitStep_PartResolveMinion : public UGSTN_HitStep_PartResolve
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;

	/** The first version (parent step + compensation, no cache). Same result as Resolve_Implementation except Out.Requests, which the cached path leaves empty */
	EGSTN_StepResult ResolveUncached(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const;

	/** Drops every cached (part set, rules) entry: call after changing a part set's table or parts, the rules' pipeline or the settings at run time */
	static void InvalidateStepCache();

	/** Switch of the cached path (also gstn.PartsMinion.StepCache). Tests flip it to compare both paths */
	static void SetStepCacheEnabled(bool bEnabled);
	static bool IsStepCacheEnabled();

	/** Time spent in this step for part-bearing victims since the process started (gstn.PartsMinion.Bench.Report) */
	static FGSTN_PartsMinionStepTiming& GetTiming();
};
