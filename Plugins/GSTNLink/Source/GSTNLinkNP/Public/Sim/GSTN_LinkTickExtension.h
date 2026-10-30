// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_LinkInteractProvider.h"
#include "Simulation/GSTN_CombatTickExtension.h"

class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_InteractMeaningTable;
struct FGSTN_LinkFragment;
struct FGSTN_LinkState;
struct FGSTN_SnapshotEntry;

/**
 * The link model's per-frame rules over the link fragment (m6 spec 5.2 steps 5 and 7, 7.4), inside the combat tick of every entity; one
 * fragment lookup for an entity that never linked. Priority 120: after the parts (100) and the defence (110) extensions, so it sees what they
 * wrote, and it reads neither of them.
 *
 *  - AfterClocks (the link tick of 7.4): a PREDICTED link (Phase Pending, flag Predicted) that got no response within the settings'
 *    RequestTimeoutFrames (12) is cleared, its serial kept; a PendingBreak a role input asked for (MarkPendingBreak, the Release command of
 *    F12) is held for the watchdog. Nothing here moves another entity or writes its state.
 *  - BlocksActions (asked after BeforeCandidates): the entity's Body role is a parked role that does not act while linked (the grabbed): no
 *    command starts, so it has no candidates. A role with bActsWhileLinked (the latcher, the latched enemy, the grabber) gets no veto and keeps
 *    its candidates: the latched enemy fights on.
 *  - AfterExecute: a grab-like hit of the entity's own attacker step this frame (a hit whose data names a link type the registry knows, on a
 *    free channel) marks the initiator's side of the link PREDICTED (Phase Pending, flag Predicted, the victim as partner, no role: m6 spec
 *    7.2 item 1, the grab confirm feel is the hitstop's); F10b's LinkResponse at F+2 turns it into role 1 or clears it.
 *  - BeforeTagProjection: the parked edge of the Body role queues FGSTN_LinkedModifier (into the parked mode) or cancels it; then the tags
 *    are SET from the fragment the frame wrote, never added: Combat.Link.Linked (Body role held), Combat.Link.Pending (a predicted link),
 *    the role tag of every channel through the registry (every registered role tag is set, so a role that ended is cleared), and
 *    Interact.Target.Grab through the GSTNLinkGAS candidate provider: an FGSTN_LinkInteractSource built from the step snapshot (end of the
 *    previous frame), the kit's grab offer, one stateless probe (no sticky focus, no line of sight: F10 deviation), then
 *    FGSTN_LinkInteractProvider::ProjectTargetTag. A linked entity, or one whose kit cannot grab, projects no target.
 *
 * Simulation code: deterministic, no net mode, no resimulation branch, state only in the fragment. The registry, the attach table and the
 * meanings are content; the tags go to the entity's ability system, which rolls back with the rest; the Mover modifier goes through Mover's
 * own queue, which the same edge re-queues in a re-simulation.
 */
class GSTNLINKNP_API FGSTN_LinkTickExtension : public IGSTN_CombatTickExtension
{
public:
	/** After the parts (100) and the defence (110) */
	static constexpr int32 ExtensionPriority = 120;

	virtual uint8 GetTypeId() const override;
	virtual int32 GetPriority() const override { return ExtensionPriority; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;
	virtual bool BlocksActions(const FGSTN_CombatTickContext& Context) const override;

	/** The registered provider of the interact probe (the module registers it with every world's interact subsystem) */
	const FGSTN_LinkInteractProvider& GetProvider() const { return Provider; }

	// ---- The rules as functions of the fragment, for the tests and the debug draw

	/** The link tick of AfterClocks: request timeouts. True when the fragment changed */
	static bool StepLinkTick(FGSTN_LinkFragment& Fragment, int32 Frame, int32 RequestTimeoutFrames);

	/** Whether the Body link makes the entity a parked role that does not act (the BlocksActions rule) */
	static bool IsActionBlocked(const FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry);

	/** Whether the Body link makes the entity a parked role (acting or not): the modifier's edge */
	static bool IsParked(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry);

	/** Marks the predicted side of a grab-like hit on the initiator's fragment: false when the channel is busy or the type unknown */
	static bool MarkPredicted(FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry& Registry, const FGameplayTag& LinkType, int32 VictimId, int32 Frame);

	/** Sets PendingBreak with a reason on a live Body link (a role input such as Release): false when the Body channel holds no live link */
	static bool MarkPendingBreak(FGSTN_LinkFragment& Fragment, uint8 Reason);

	/** The Combat.Link.* tags the fragment projects (Linked, Pending, the role tag of each channel) */
	static void GatherStateTags(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry, FGameplayTagContainer& Out);

	/** Every tag GatherStateTags can project (Linked, Pending, every registered role tag): the ones the projection sets to zero when absent */
	static void GatherProjectableTags(const UGSTN_LinkRegistry* Registry, FGameplayTagContainer& Out);

	/**
	 * The grab focus of an entity: Interact.Target.Grab (or nothing) from the probe over Snapshot, with Self as the interactor. Meanings must hold
	 * the grab meaning. Pure: the snapshot entries, their link extensions, the registry and the attach table
	 */
	FGameplayTag ResolveGrabTarget(const FGSTN_SnapshotEntry& Self, TConstArrayView<FGSTN_SnapshotEntry> Entries, int32 Frame, const FVector& StickOnPlane,
		const FGSTN_InteractMeaningTable& Meanings) const;

	/** The meaning table the probe uses: the grab meaning (and the interact root), built once */
	static const FGSTN_InteractMeaningTable& GetLinkMeanings();

private:
	void ProjectTags(FGSTN_CombatTickContext& Context) const;

	FGSTN_LinkInteractProvider Provider;
};
