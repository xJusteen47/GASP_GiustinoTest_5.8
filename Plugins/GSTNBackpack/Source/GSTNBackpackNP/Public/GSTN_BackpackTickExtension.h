// Backpacks, Network Prediction half (M11 L7b).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatTickExtension.h"

struct FGSTN_BackpackFragment;
struct FGSTN_CombatSyncCore;

/**
 * Runs the backpack's memory slots and dome inside the combat tick (FGSTN_BackpackStep on FGSTN_BackpackFragment). Priority 140, after the
 * spin and drill (130). Simulation code: deterministic, no net mode, no resimulation branch, state only in the fragment.
 *
 *  - AfterClocks: OverTime refills; a dome whose move ended (WhileHeld) or whose frames ran out (Deploy) falls.
 *  - AfterExecute: a landing of this frame (the jump state went from airborne to none) refills the OnLanding slots; a timeline that started
 *    this frame and carries a FGSTN_BackpackChargeTrack spends, one with a FGSTN_BackpackDomeTrack raises the dome where the entity stood at
 *    the end of the previous frame (its step snapshot entry).
 *  - BeforeTagProjection: Backpack.State.Empty.Slot<N> and Backpack.State.Dome SET from the fragment, never added.
 *
 * Every rule returns the fragment to its defaults once nothing is live, so an entity whose backpack is idle costs no payload.
 */
class GSTNBACKPACKNP_API FGSTN_BackpackTickExtension : public IGSTN_CombatTickExtension
{
public:
	static constexpr int32 ExtensionPriority = 140;

	virtual uint8 GetTypeId() const override;
	virtual int32 GetPriority() const override { return ExtensionPriority; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;

	/** The WhileHeld dome's move still runs: the combat state runs the timeline that raised it, from the frame it raised it */
	static bool IsDomeTimelineRunning(const FGSTN_BackpackFragment& Fragment, const FGSTN_CombatSyncCore& Core);

	/** The jump state says the entity landed this frame (airborne at the end of the previous frame, on the ground now) */
	static bool HasLanded(const FGSTN_CombatSyncCore& In, const FGSTN_CombatSyncCore& Out);

protected:
	/** The frame data of a timeline that started on this frame, null otherwise (content of the table set: the same on every machine) */
	virtual const FGSTN_CombatTimelineData* ResolveStartedTimeline(const FGSTN_CombatTickContext& Context) const;

private:
	void ProjectTags(FGSTN_CombatTickContext& Context) const;
};
