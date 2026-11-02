// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_LinkInteractProvider.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Simulation/GSTN_HitInbox.h"

class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_LinkActionInfo;
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
 *  - AfterClocks, M7 G2: the PartnerMove keys of the running move start partner moves (the initiator's record, the partner's entry posted to its
 *    inbox); StepPartnerMoves starts a scheduled partner part (StartForcedTimeline, source Forced), applies the initiator's outcome at its
 *    frame and aborts on the failure paths (Combat.Event.Link.PartnerMoveAborted).
 *  - AfterClocks, M7 G6 (a type with bInitiatorPlacesExit): first, a parked initiator whose own exit is due this frame ends its link by itself
 *    (StepOwnExitBreak: what the watchdog's command does, which on the authority already did it), so its machine shows the break and the
 *    teleport on the frame the authority does; last, a parked initiator whose Body link holds a PendingBreak and no exit yet places its exit
 *    (StepInitiatorExit) around its partner in IGSTN_CombatTickServices::GetViewSnapshot (Context.Snapshot without services), due the next frame.
 *  - AfterExecute, M7 G2: StepFacing turns a parked role that acts (LocalYawQ) from its aimed moves and idle stick.
 *  - BlocksActions (asked after BeforeCandidates): the entity's Body role is a parked role that does not act while linked (the grabbed), or it
 *    plays the partner's part of a partner move that locks commands (M7 G2): no command starts, so it has no candidates. A role with bActsWhileLinked (the latcher, the latched enemy, the grabber) gets no veto and keeps
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

	// ---- Initiator-placed exits (M7 G6)

	/** What the exiting entity needs of itself to place its exit: its up and its capsule (content, the same on every machine) */
	struct FExitSelf
	{
		FVector Up = FVector::UpVector;
		float CapsuleRadius = 34.f;
		float CapsuleHalfHeight = 88.f;
	};

	/**
	 * The parked initiator of a type with bInitiatorPlacesExit places its own exit: when its Body link is Active, holds a PendingBreak and its
	 * link actions hold no exit, the exit placer (the playing partner move's, else the type's: what the watchdog would use) runs without
	 * collision around PartnerInView (the partner's entry of the initiator's view snapshot), quantised as the watchdog's command carries it,
	 * and Actions.Exit = {Frame + 1, the place, Teleport | InitiatorPlaced}. False (nothing changed) for any other link or without the partner
	 */
	static bool StepInitiatorExit(FGSTN_LinkFragment& Fragment, int32 Frame, const UGSTN_LinkRegistry* Registry, const FGSTN_SnapshotEntry* PartnerInView,
		const FExitSelf& Self);

	/**
	 * The frame an initiator-placed exit is due (Actions.Exit.Frame == Frame, InitiatorPlaced) on a Body link that still holds its PendingBreak:
	 * the link ends exactly as the watchdog's Break command stamped Frame - 1 would end it (FGSTN_LinkInboxHandlers::ApplyCommand with that
	 * exit and the pending reason). Where the command was applied first (the authority drains it before this point) the link is already idle and
	 * nothing happens. True when the fragment changed
	 */
	static bool StepOwnExitBreak(FGSTN_LinkFragment& Fragment, int32 Frame, const UGSTN_LinkRegistry* Registry);

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

	// ---- Partner moves (M7 G2, M7 spec 6.5)

	/** A partner move the outcome keys started: the entry and the partner it goes to (the hook posts it to the partner's inbox, stamped this frame) */
	struct FPartnerMovePost
	{
		int32 PartnerId = INDEX_NONE;
		FGSTN_LinkPartnerMoveEntry Entry;
	};

	/**
	 * The initiator's side of a partner move at frame S (its move crossed a PartnerMove key): the link actions record the move (Active = {Id, S,
	 * Initiator}) and OutEntry is the partner's part {Serial, Id, TargetFrame = S + max(PartnerStartOffsetFrames, 1)}. False (nothing changed) when
	 * the Body link is not Active or a partner move already plays: one at a time
	 */
	static bool StartPartnerMove(FGSTN_LinkFragment& Fragment, int16 Id, int32 Frame, const FGSTN_LinkActionInfo& Info, FGSTN_LinkPartnerMoveEntry& OutEntry);

	/**
	 * The PartnerMove keys the running move crossed this frame (like StepOutcomeKeys, Body link only): each resolved through the type's action
	 * provider and started by StartPartnerMove, its post handed back. A key whose id the provider does not know is ignored. Needs Context.RunningTimeline
	 */
	static bool StepPartnerMoveKeys(const FGSTN_CombatTickContext& Context, FGSTN_LinkFragment& Fragment, TArray<FPartnerMovePost, TInlineAllocator<1>>& OutPosts);

	/** What StepPartnerMoves reads of the entity's frame: values from the combat state and the frame's inputs, so the tests need no model */
	struct FPartnerMoveFrame
	{
		int32 Frame = 0;
		/** The entity's own state tags at the end of the previous frame (what the snapshot reads), for the partner requirements */
		FGameplayTagContainer OwnTags;
		/** Knocked out (EGSTN_ActionState::KO) */
		bool bKnockedOut = false;
		/** The entity can be given a forced move: Actionable or in a timeline (not in a hit reaction) */
		bool bCanStart = true;
		/** The running timeline of Out: whether one runs, the frame it started on, and the timeline frames this frame's clock step crossed */
		bool bInTimeline = false;
		int32 TimelineStartFrame = INDEX_NONE;
		bool bTimelineCrossed = false;
		int32 TimelinePrevious = -1;
		int32 TimelineCurrent = -1;
	};

	struct FPartnerMoveResult
	{
		/** The partner ability to start this frame (source Forced), or null */
		const UClass* StartAbility = nullptr;
		/** A move was aborted this frame (its id): the hook posts Combat.Event.Link.PartnerMoveAborted */
		int16 AbortedId = 0;
	};

	/**
	 * The partner moves of AfterClocks (M7 G2), on the entity's Body link:
	 *  - a scheduled part (PendingStart) whose frame has come starts: Active = {Id, Frame, Partner} and OutResult.StartAbility; it is dropped
	 *    (with an abort event, the link kept) when the provider does not know it, the requirements fail or the entity is in a hit reaction;
	 *  - the initiator's move reaching its OutcomeFrame (its own timeline, same run, hitstop-aware): Break -> PendingBreak with the action's reason
	 *    (the watchdog breaks both sides with the action's exit; the move stays recorded until then), KeepLink -> the move is over, SwapRoles ->
	 *    reserved (logged once, as KeepLink). An initiator whose move ended before its outcome (replaced, hit) ends its record;
	 *  - a partner whose forced part ended (its timeline replaced or over) ends its record, which lifts its command lock;
	 *  - failure paths: a knocked-out side, or a move older than MaxDurationFrames, aborts: the record ends, PendingBreak with Forced (the watchdog
	 *    breaks with the type's default exit), OutResult.AbortedId.
	 * With no live Body link any record is dropped. True when the fragment changed
	 */
	static bool StepPartnerMoves(FGSTN_LinkFragment& Fragment, const FPartnerMoveFrame& In, const UGSTN_LinkRegistry* Registry, FPartnerMoveResult& OutResult);

	/** The partner of a partner move whose action locks commands (bPartnerLocksCommands) plays its part: part of the BlocksActions rule */
	static bool IsCommandLocked(const FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry);

	// ---- Parked facing (M7 G2)

	/**
	 * The turn of a parked role that acts (bActsWhileLinked, YawLimitDeg > 0) at AfterExecute: on the frame one of its moves starts (an aimed
	 * move) and on every frame it is idle, LocalYawQ faces StickOnPlane relative to Anchor (its parked transform without a turn, from the step
	 * snapshot) within the role's YawLimitDeg; no stick keeps the turn. Any other role, or a link that is not Active, is left alone. True when
	 * the fragment changed
	 */
	static bool StepFacing(FGSTN_LinkFragment& Fragment, const UGSTN_LinkRegistry* Registry, const FTransform& Anchor, const FVector& StickOnPlane, bool bIdle, bool bMoveStarted);

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
