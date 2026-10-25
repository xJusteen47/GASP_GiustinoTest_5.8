// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Effects/GSTN_GEComponent_RestoreParts.h"
#include "Math/GSTN_PartsProjection.h"
#include "Simulation/GSTN_CombatTickExtension.h"

class UGameplayEffect;
class UGSTN_GE_PartStats;
class UGSTN_PartSetDefinition;

/**
 * What the broken mask means for the rest of the combat, recomputed from the fragment every frame (combat-parts.md §3.5, §5.3, §7).
 * Nothing it derives is stored, so a rollback has nothing to undo: a re-simulated frame ends with the same tags and the same masked
 * hit groups.
 *
 *  - BeforeCandidates: the hit groups of the entity's own hitboxes that broken parts disable (DisableHitGroups) go into the context's
 *    HitGroupMask, read from the fragment at the END OF F-1 (the state the frame started from). A masked group generates no hitbox: no
 *    hit, no hitstop, no confirm.
 *  - BeforeTagProjection: the derived tags (Combat.Part.Broken, GrabLocked, AllBroken and the tags the Tags break effects grant) are SET
 *    on the ability system from the state the frame wrote, through the P5 helper, so Mover sees them in the same frame (CLAUDE.md rule 5).
 *    With a stats effect class set, the Stats break effects are a projection of the mask too: applied for every broken part (idempotent)
 *    and removed for every part that is not broken, which also undoes a break a rollback took back.
 *
 *  - AfterClocks: the restore requests (effects carrying the restore component) on the ability system are consumed: the fragment is restored
 *    (UGSTN_PartMath::RestoreParts), Combat.Event.Part.Restored goes to the frame's outbox and the effects are removed. This runs after the hits
 *    of the frame were applied, so a hit and a restore of one frame resolve in that order. A single-part set (a player) that is knocked out is
 *    not restored: KO wins. The effects ARE the pending requests, see UGSTN_GEComponent_RestoreParts.
 *
 * It follows the rules of simulation code: deterministic, no net mode, no resimulation branch, state only in the fragment. The set and
 * the ability system are content the owner has (FGSTN_PartsAttach, the tick context), never state. It runs for every entity: one without
 * a part set is skipped after one map lookup.
 *
 * The hooks of all registered extensions run in the order of (priority, type id). Priority is explicit and fixed so the order never
 * depends on the load order of the plugins: parts run after the default extensions (priority 0) so they see what those wrote.
 */
class GSTNPARTSNP_API FGSTN_PartsTickExtension : public IGSTN_CombatTickExtension
{
public:
	/** After the default priority 0, so it sees what the other extensions wrote */
	static constexpr int32 ExtensionPriority = 100;

	virtual uint8 GetTypeId() const override;
	virtual int32 GetPriority() const override { return ExtensionPriority; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;

	/** The infinite effect the Stats break effects apply while their part is broken; null = stats are not projected */
	void SetStatsEffectClass(TSubclassOf<UGSTN_GE_PartStats> InClass);

	TSubclassOf<UGSTN_GE_PartStats> GetStatsEffectClass() const { return StatsEffectClass; }

private:
	void ConsumeRestores(FGSTN_CombatTickContext& Context, const UGSTN_PartSetDefinition& Set);

	/** Restore requests of the frame, scratch like the projection */
	TArray<FGSTN_PendingPartRestore> PendingRestores;

	/** Scratch kept between calls so the steady state allocates nothing. Not state: rewritten from the fragment at every call */
	FGSTN_PartsProjection Scratch;

	TSubclassOf<UGSTN_GE_PartStats> StatsEffectClass;
};
