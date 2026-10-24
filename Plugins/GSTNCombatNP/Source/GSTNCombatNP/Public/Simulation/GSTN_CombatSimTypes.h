// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Jump/GSTN_JumpTypes.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatHitState.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "Sync/GSTN_SyncFragment.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTrace.h"
#include "Misc/StringBuilder.h"

/**
 * The fixed part of the combat sync state, present on every combat entity: the embedded input simulation state plus the
 * debug hitstop window (the hitstop frames are Network Prediction frames, like the input state's). What a feature adds on top
 * of it goes in a sync fragment, not here.
 */
struct FGSTN_CombatSyncCore
{
	/** History, classifier, watched tags and the frame-stamped ability request */
	FGSTN_InputSimState Input;

	/** Hitstop window [HitstopStartFrame, HitstopEndFrame) */
	int32 HitstopStartFrame = INDEX_NONE;
	int32 HitstopEndFrame = INDEX_NONE;

	/** Jump phase, air jumps used, coyote window and the hold of the jump button (combat-m3 7.9). Written by the combat tick only; one bit when default */
	FGSTN_JumpState Jump;

	bool IsInHitstop(int32 Frame) const { return Frame >= HitstopStartFrame && Frame < HitstopEndFrame; }

	/**
	 * The move clock of a frame (combat-m4-hits 8): a frame inside the window of the previous state adds one to PausedFramesTotal, so
	 * the combat clock (frame minus paused frames) stands still. Returns whether the frame was a hitstop frame. Deterministic: it reads
	 * and writes this state only
	 */
	bool AdvanceHitstopClock(int32 Frame)
	{
		const bool bHitstopFrame = IsInHitstop(Frame);
		PausedFramesTotal += bHitstopFrame ? 1 : 0;
		return bHitstopFrame;
	}

	// ---- The action part (combat-m3 7.1): what the combatant is doing, the timeline it runs and the requests of this frame.
	// An idle combatant holds the defaults and costs one bit on the wire.

	EGSTN_ActionState ActionState = EGSTN_ActionState::Actionable;

	/** Index in the timeline registry (the compiled binding table), INDEX_NONE when none */
	int16 ActiveTimeline = INDEX_NONE;
	EGSTN_ActivationSource ActiveSource = EGSTN_ActivationSource::Command;

	/** Timeline frame = F - TimelineStartFrame - TimelinePausedFrames (the paused frames grow in hitstop) */
	int32 TimelineStartFrame = INDEX_NONE;
	int32 TimelinePausedFrames = 0;

	/** Every hitstop frame of the combatant so far: combat clock = F - PausedFramesTotal. Constant outside hitstop */
	int32 PausedFramesTotal = 0;

	/** 0 = the primary weapon, 1.. = the additional weapons in loadout order */
	uint8 ActiveWeaponIndex = 0;

	/** Written only by the server, out of band (UGSTN_CombatSimComponent::ServerWriteLoadoutRevision): the revision of the loadout the simulation is to run on (the demand) */
	uint16 LoadoutRevision = 0;

	/**
	 * The revision of the table set the simulation has swapped to (FGSTN_LoadoutSwapLogic). It follows LoadoutRevision at the first tick
	 * that has the demanded table, so the swap happens on one frame on every machine; a state that carries both equal needs no swap
	 */
	uint16 TableRevision = 0;

	/** The requests of the frame being simulated; PruneBefore at the top of every frame keeps older ones off the wire */
	FGSTN_CombatRequests Requests;

	/**
	 * The hit part (combat-m4-hits 11): the attacker's targets and move origin, the victim's windows and pending launch. Its fields
	 * expire by their own end frames, so it belongs to the action part only while something in it is live (IsActionPartDefault)
	 */
	FGSTN_HitSyncState Hit;

	/** The frame and timeline facts the hit part needs to tell live fields from expired ones */
	FGSTN_HitLiveContext GetHitContext() const
	{
		FGSTN_HitLiveContext Context;
		Context.Frame = Input.History.GetLatestFrame();
		Context.bInTimeline = ActionState == EGSTN_ActionState::InTimeline;
		Context.TimelineStartFrame = TimelineStartFrame;
		Context.HitstopEndFrame = HitstopEndFrame;
		return Context;
	}

	/** True when every field of the action part holds its default (an idle combatant that never had a loadout applied) */
	bool IsActionPartDefault() const
	{
		return ActionState == EGSTN_ActionState::Actionable && ActiveTimeline == INDEX_NONE && ActiveSource == EGSTN_ActivationSource::Command
			&& TimelineStartFrame == INDEX_NONE && TimelinePausedFrames == 0 && PausedFramesTotal == 0 && ActiveWeaponIndex == 0
			&& LoadoutRevision == 0 && TableRevision == 0 && Requests.IsEmpty() && Hit.IsDefault(GetHitContext());
	}

	/**
	 * The loadout revision gate (combat-m3 7.8): the combat tick makes requests and adopts abilities only while the revision in
	 * the state is the one the local table was built from. Until the table arrives the history and the buffer still run, so the
	 * presses wait in the buffer.
	 */
	bool IsLoadoutCurrent(uint16 LocalTableRevision) const { return LoadoutRevision == LocalTableRevision; }

	void NetSerialize(FArchive& Ar)
	{
		Input.NetSerialize(Ar);
		Ar << HitstopStartFrame;
		Ar << HitstopEndFrame;
		Jump.NetSerialize(Ar);

		uint8 bAction = IsActionPartDefault() ? 0 : 1;
		Ar.SerializeBits(&bAction, 1);
		if (!bAction)
		{
			if (Ar.IsLoading())
			{
				ResetActionPart();
			}
			return;
		}

		uint8 Action = static_cast<uint8>(ActionState);
		Ar.SerializeBits(&Action, 3);
		uint8 Source = static_cast<uint8>(ActiveSource);
		Ar.SerializeBits(&Source, 2);
		// +1 so that INDEX_NONE (-1) is 0 and the packed form stays small
		uint32 TimelinePlusOne = static_cast<uint32>(FMath::Max(static_cast<int32>(ActiveTimeline) + 1, 0));
		Ar.SerializeIntPacked(TimelinePlusOne);
		uint32 StartPlusOne = static_cast<uint32>(FMath::Max(TimelineStartFrame + 1, 0));
		Ar.SerializeIntPacked(StartPlusOne);
		uint32 Paused = static_cast<uint32>(FMath::Max(TimelinePausedFrames, 0));
		Ar.SerializeIntPacked(Paused);
		uint32 PausedTotal = static_cast<uint32>(FMath::Max(PausedFramesTotal, 0));
		Ar.SerializeIntPacked(PausedTotal);
		Ar << ActiveWeaponIndex;
		Ar << LoadoutRevision;
		Ar << TableRevision;
		Requests.NetSerialize(Ar);

		if (Ar.IsLoading())
		{
			if (Action >= static_cast<uint8>(EGSTN_ActionState::Count) || Source > static_cast<uint8>(EGSTN_ActivationSource::Other) || TimelinePlusOne > 0x8000u)
			{
				Ar.SetError();
				ResetActionPart();
				return;
			}
			ActionState = static_cast<EGSTN_ActionState>(Action);
			ActiveSource = static_cast<EGSTN_ActivationSource>(Source);
			ActiveTimeline = static_cast<int16>(static_cast<int32>(TimelinePlusOne) - 1);
			TimelineStartFrame = static_cast<int32>(StartPlusOne) - 1;
			TimelinePausedFrames = static_cast<int32>(Paused);
			PausedFramesTotal = static_cast<int32>(PausedTotal);
		}

		// After the fields it reads its context from: the loaded state has them by now
		Hit.NetSerialize(Ar, GetHitContext());
	}

	void ResetActionPart()
	{
		ActionState = EGSTN_ActionState::Actionable;
		ActiveTimeline = INDEX_NONE;
		ActiveSource = EGSTN_ActivationSource::Command;
		TimelineStartFrame = INDEX_NONE;
		TimelinePausedFrames = 0;
		PausedFramesTotal = 0;
		ActiveWeaponIndex = 0;
		LoadoutRevision = 0;
		TableRevision = 0;
		Requests = FGSTN_CombatRequests();
		Hit = FGSTN_HitSyncState();
	}

	bool ShouldReconcile(const FGSTN_CombatSyncCore& AuthorityState) const
	{
		// The input state is authoritative like the rest: a mismatch asks for a reconcile (the field is traced by the call)
		if (Input.ShouldReconcile(AuthorityState.Input))
		{
			return true;
		}
		UE_NP_TRACE_RECONCILE(HitstopStartFrame != AuthorityState.HitstopStartFrame, "HitstopStartFrame:");
		UE_NP_TRACE_RECONCILE(HitstopEndFrame != AuthorityState.HitstopEndFrame, "HitstopEndFrame:");
		UE_NP_TRACE_RECONCILE(!(Jump == AuthorityState.Jump), "Jump:");
		UE_NP_TRACE_RECONCILE(ActionState != AuthorityState.ActionState, "ActionState:");
		UE_NP_TRACE_RECONCILE(ActiveTimeline != AuthorityState.ActiveTimeline, "ActiveTimeline:");
		UE_NP_TRACE_RECONCILE(ActiveSource != AuthorityState.ActiveSource, "ActiveSource:");
		UE_NP_TRACE_RECONCILE(TimelineStartFrame != AuthorityState.TimelineStartFrame, "TimelineStartFrame:");
		UE_NP_TRACE_RECONCILE(TimelinePausedFrames != AuthorityState.TimelinePausedFrames, "TimelinePausedFrames:");
		UE_NP_TRACE_RECONCILE(PausedFramesTotal != AuthorityState.PausedFramesTotal, "PausedFramesTotal:");
		UE_NP_TRACE_RECONCILE(ActiveWeaponIndex != AuthorityState.ActiveWeaponIndex, "ActiveWeaponIndex:");
		UE_NP_TRACE_RECONCILE(LoadoutRevision != AuthorityState.LoadoutRevision, "LoadoutRevision:");
		UE_NP_TRACE_RECONCILE(TableRevision != AuthorityState.TableRevision, "TableRevision:");
		UE_NP_TRACE_RECONCILE(!(Requests == AuthorityState.Requests), "Requests:");
		// Compared in canonical form: a window that ran out is not a difference, and the quantised fields compare as integers
		UE_NP_TRACE_RECONCILE(!(Hit.Canonical(GetHitContext()) == AuthorityState.Hit.Canonical(AuthorityState.GetHitContext())), "Hit:");
		return false;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Input.ToString(Out);
		Out.Appendf(" Hitstop: [%d, %d)", HitstopStartFrame, HitstopEndFrame);
		if (!Jump.IsDefault())
		{
			Jump.ToString(Out);
		}
		if (!IsActionPartDefault())
		{
			Out.Appendf(" Action: %d Timeline: %d@%d+%d Source: %d Paused: %d Weapon: %d Loadout: %d Table: %d", static_cast<int32>(ActionState), static_cast<int32>(ActiveTimeline), TimelineStartFrame, TimelinePausedFrames, static_cast<int32>(ActiveSource), PausedFramesTotal, static_cast<int32>(ActiveWeaponIndex), static_cast<int32>(LoadoutRevision), static_cast<int32>(TableRevision));
			Requests.ToString(Out);
			Hit.Canonical(GetHitContext()).ToString(Out);
		}
		Out.Append("\n");
	}
};

/**
 * State of the combat model, rolled back and re-simulated by Network Prediction: the fixed core plus the sync fragments of
 * the features that are present on this entity (shield meter, link role...; see FGSTN_SyncFragment). The fragment list is
 * written after the core, so the core's bytes are the same with or without fragments, and an entity without any pays one
 * bit for them (FGSTN_SyncFragmentList documents the format).
 */
struct FGSTN_CombatSyncState
{
	FGSTN_CombatSyncCore Core;

	/** Sorted by type id; use FGSTN_SyncFragmentList to read and add */
	FGSTN_SyncFragmentArray Fragments;

	void NetSerialize(const FNetSerializeParams& P)
	{
		Core.NetSerialize(P.Ar);
		FGSTN_SyncFragmentList::NetSerialize(P.Ar, Fragments);
	}

	bool ShouldReconcile(const FGSTN_CombatSyncState& AuthorityState) const
	{
		if (Core.ShouldReconcile(AuthorityState.Core))
		{
			return true;
		}
		UE_NP_TRACE_RECONCILE(FGSTN_SyncFragmentList::ShouldReconcile(Fragments, AuthorityState.Fragments), "Fragments:");
		return false;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Core.ToString(Out);
		FGSTN_SyncFragmentList::ToString(Fragments, Out);
	}

	/** The fixed part is discrete: interpolated proxies just take the newer frame, except the move origin of the hit part, which blends. Fragments may blend (FGSTN_SyncFragment::Interpolate) */
	void Interpolate(const FGSTN_CombatSyncState* From, const FGSTN_CombatSyncState* To, float PCT)
	{
		Core = To->Core;
		Core.Hit = FGSTN_HitSyncState::Interpolated(From ? &From->Core.Hit : nullptr, To->Core.Hit, PCT);
		FGSTN_SyncFragmentList::Interpolate(Fragments, From ? &From->Fragments : nullptr, To->Fragments, PCT);
	}
};

/** No auxiliary state yet */
struct FGSTN_CombatAuxState
{
	void NetSerialize(const FNetSerializeParams& P) {}
	bool ShouldReconcile(const FGSTN_CombatAuxState& AuthorityState) const { return false; }
	void ToString(FAnsiStringBuilderBase& Out) const {}
	void Interpolate(const FGSTN_CombatAuxState* From, const FGSTN_CombatAuxState* To, float PCT) {}
};

using FGSTN_CombatStateTypes = TNetworkPredictionStateTypes<FGSTN_InputCmd, FGSTN_CombatSyncState, FGSTN_CombatAuxState>;
