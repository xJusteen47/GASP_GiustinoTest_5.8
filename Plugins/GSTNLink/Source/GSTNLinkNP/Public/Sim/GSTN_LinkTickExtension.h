// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_LinkInteractProvider.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_HitInbox.h"

class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_InteractMeaningTable;
struct FGSTN_LinkBodyData;
struct FGSTN_LinkFragment;
struct FGSTN_LinkSnapshotExt;
struct FGSTN_LinkKitExtension;
struct FGSTN_LinkState;
struct FGSTN_SnapshotEntry;

/**
 * The link model's per-frame rules over the link fragment (m6 spec 5.2 steps 5 and 7, 7.4), inside the combat tick of every entity; one
 * fragment lookup for an entity that never linked. Priority 120: after the parts (100) and the defence (110) extensions, so it sees what they
 * wrote, and it reads neither of them.
 *
 *  - AfterClocks (the link tick of 7.4): for a type with bInitiatorPredicts (M7 G1), a predicted request PendingFrames old is accepted
 *    optimistically (StepOptimisticAccept: Active, role 1, flag Predicted kept) unless the target's snapshot entry shows it linked to someone
 *    else; a PREDICTED link (Phase Pending, flag Predicted) that got no response within the settings'
 *    RequestTimeoutFrames (12) is cleared, its serial kept; a PendingBreak a role input asked for (MarkPendingBreak, the Release command of
 *    F12) is held for the watchdog; on the frame a non-acting parked role starts (the grabbed), its running timeline is cancelled, so no
 *    hitbox of it lands on the partner. Nothing here moves another entity or writes its state.
 *  - BlocksActions (asked after BeforeCandidates): the entity's Body role is a parked role that does not act while linked (the grabbed): no
 *    command starts, so it has no candidates. A role with bActsWhileLinked (the latcher, the latched enemy, the grabber) gets no veto and keeps
 *    its candidates: the latched enemy fights on.
 *  - AfterExecute: a grab-like hit of the entity's own attacker step this frame (a hit whose data names a link type the registry knows, on a
 *    free channel) marks the initiator's side of the link PREDICTED (Phase Pending, flag Predicted, the victim as partner, no role: m6 spec
 *    7.2 item 1, the grab confirm feel is the hitstop's); F10b's LinkResponse at F+2 turns it into role 1 or clears it. For a type that
 *    predicts (M7 G1) the request is resolved on the same snapshot (PredictRequest: the alternative that forms and the rules' Payload), so the
 *    optimistic accept holds what the target will form.
 *  - BeforeTagProjection: the parked edge of the Body role queues FGSTN_LinkedModifier (into the parked mode) or cancels it; the link effects
 *    of the roles that became Active or ended this frame are applied or removed (FGSTN_LinkEffects, M7 G1); then the tags
 *    are SET from the fragment the frame wrote, never added: Combat.Link.Linked (Body role held), Combat.Link.Pending (a predicted link),
 *    the role tag of every channel through the registry (every registered role tag is set, so a role that ended is cleared), and the
 *    Interact.Target meaning of the focus through the GSTNLinkGAS candidate provider: an FGSTN_LinkInteractSource built from the step snapshot
 *    (end of the previous frame), the offers the registry compiled for the kit (M7 G1: the grab and its alternatives, then any plugin's type),
 *    one stateless probe over the registry's meaning table (no sticky focus, no line of sight: F10 deviation), then
 *    FGSTN_LinkInteractProvider::ProjectTargetTag. A linked entity, or one whose kit no offer applies to, projects no
 *    target: the probe reads other entities' snapshot positions, interpolated on a client, so the tag is not prediction-stable (an F12 item).
 *    On the frame a LinkCommand's exit record is written with the Teleport flag (the watchdog's break, F11) the exit request applies the
 *    teleport (FGSTN_LinkExitRequest), in the same Mover tick as the parked mode's end. The exit record is dropped the frame after its own,
 *    and a grab immunity that has run out (AfterClocks), so an idle entity is idle on the wire.
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

	/** A throw the outcome keys decided: the entry and the partner it goes to (the hook posts it through the combat world, as the attacker step does) */
	struct FThrowPost
	{
		int32 PartnerId = INDEX_NONE;
		FGSTN_HitInboxEntry Entry;
	};

	/**
	 * The link outcome keys the running move crossed this frame (F12, FGSTN_LinkOutcomeKeys) for the entity's live Body link: Release sets
	 * PendingBreak with the role's release reason (FGSTN_LinkShakeOff), Throw ends the own link and hands the throw entry back in OutThrows,
	 * KeepLink does nothing, SwapRoles is not built (logged once). Needs Context.RunningTimeline; the throw needs both entities in Context.Snapshot.
	 * True when the fragment changed
	 */
	static bool StepOutcomeKeys(const FGSTN_CombatTickContext& Context, FGSTN_LinkFragment& Fragment, TArray<FThrowPost, TInlineAllocator<1>>& OutThrows);

	/** The grabbed side's mash at AfterClocks (FGSTN_LinkMash), on the input history of the end of the previous frame. True when the fragment changed */
	static bool StepMash(const FGSTN_CombatTickContext& Context, FGSTN_LinkFragment& Fragment);

	/**
	 * The optimistic accept of AfterClocks (M7 G1), for a channel whose predicted request (Pending, flag Predicted) is of a type with
	 * bInitiatorPredicts and is PendingFrames old: Active, role 1, flag Predicted kept, unless the target's snapshot entry (end of the previous
	 * frame; TargetExtOf gives it by entity id, null when the target is not in the snapshot) shows its channel linked to someone else, which
	 * clears the request as a refusal would. When the target already holds role 2 with this entity, the state takes its serial and link frame
	 * (exactly the authority's); otherwise the serial is the one the target will form (one after the newer of both, on the Body channel) and the
	 * link frame the request frame + 1. The Payload PredictRequest wrote stays. Types without the flag (grab, latch) are left alone. SelfId is
	 * the entity's own id. True when the fragment changed
	 */
	static bool StepOptimisticAccept(FGSTN_LinkFragment& Fragment, int32 SelfId, int32 Frame, const UGSTN_LinkRegistry* Registry,
		TFunctionRef<const FGSTN_LinkSnapshotExt*(int32 EntityId)> TargetExtOf);

	/**
	 * Resolves a predicting type's request on the initiator's own machine (M7 G1), on the step snapshot its attacker step used: the request
	 * context the target's LinkRequest step will build (FGSTN_LinkRequestContext from the two snapshot entries), the asked type's Alternatives
	 * then the type, and the first that accepts. Writes that type's index (when it uses State's channel) and its rules' Payload into the
	 * Pending State. False (State unchanged) when nothing accepts: the request then waits for the response or its timeout
	 */
	static bool PredictRequest(FGSTN_LinkState& State, const UGSTN_LinkRegistry& Registry, const FGSTN_SnapshotEntry& Self, const FGSTN_SnapshotEntry& Victim,
		const FGameplayTag& AskedType, int32 Frame);

	/**
	 * The request side of a snapshot entry, as the LinkRequest step builds it from the same bytes: id, team, weight, request tags, immunity and
	 * cooldown frames, link states, and the kit extension array of its body content
	 */
	static FGSTN_LinkRequestSide MakeRequestSide(const FGSTN_SnapshotEntry& Entry);

	/** The interact offers of an entity from its body content (FGSTN_LinkInteractProvider::MakeOffers with its kit); none without body data */
	static void MakeOffersFor(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkBodyData* Body, TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>>& OutOffers);

	/** The Combat.Link.* tags the fragment projects (Linked, Pending, the role tag of each channel) */
	static void GatherStateTags(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry, FGameplayTagContainer& Out);

	/** Every tag GatherStateTags can project (Linked, Pending, every registered role tag): the ones the projection sets to zero when absent */
	static void GatherProjectableTags(const UGSTN_LinkRegistry* Registry, FGameplayTagContainer& Out);

	/**
	 * The interact focus of an entity (M7 G1, was the grab focus): the Interact.Target meaning (or nothing) the probe projects over Snapshot,
	 * with Self as the interactor, the offers the registry compiled for its kit and the registry's meaning table. Pure: the snapshot entries,
	 * their link extensions, the registry and the attach table
	 */
	FGameplayTag ResolveInteractTarget(const FGSTN_SnapshotEntry& Self, TConstArrayView<FGSTN_SnapshotEntry> Entries, int32 Frame, const FVector& StickOnPlane) const;

private:
	void ProjectTags(FGSTN_CombatTickContext& Context) const;

	FGSTN_LinkInteractProvider Provider;
};
