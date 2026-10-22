// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
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

	bool IsInHitstop(int32 Frame) const { return Frame >= HitstopStartFrame && Frame < HitstopEndFrame; }

	void NetSerialize(FArchive& Ar)
	{
		Input.NetSerialize(Ar);
		Ar << HitstopStartFrame;
		Ar << HitstopEndFrame;
	}

	bool ShouldReconcile(const FGSTN_CombatSyncCore& AuthorityState) const
	{
		Input.ShouldReconcile(AuthorityState.Input);
		UE_NP_TRACE_RECONCILE(HitstopStartFrame != AuthorityState.HitstopStartFrame, "HitstopStartFrame:");
		UE_NP_TRACE_RECONCILE(HitstopEndFrame != AuthorityState.HitstopEndFrame, "HitstopEndFrame:");
		return false;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Input.ToString(Out);
		Out.Appendf(" Hitstop: [%d, %d)\n", HitstopStartFrame, HitstopEndFrame);
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

	/** The fixed part is discrete: interpolated proxies just take the newer frame. Fragments may blend (FGSTN_SyncFragment::Interpolate) */
	void Interpolate(const FGSTN_CombatSyncState* From, const FGSTN_CombatSyncState* To, float PCT)
	{
		Core = To->Core;
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
