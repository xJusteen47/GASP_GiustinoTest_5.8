// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_InputSimTypes.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTrace.h"
#include "Misc/StringBuilder.h"

/**
 * State of the combat model, rolled back and re-simulated by Network Prediction: the embedded input simulation state plus
 * the debug hitstop window (the hitstop frames are Network Prediction frames, like the input state's).
 */
struct FGSTN_CombatSyncState
{
	/** History, classifier, watched tags and the frame-stamped ability request */
	FGSTN_InputSimState Input;

	/** Hitstop window [HitstopStartFrame, HitstopEndFrame) */
	int32 HitstopStartFrame = INDEX_NONE;
	int32 HitstopEndFrame = INDEX_NONE;

	bool IsInHitstop(int32 Frame) const { return Frame >= HitstopStartFrame && Frame < HitstopEndFrame; }

	void NetSerialize(const FNetSerializeParams& P)
	{
		Input.NetSerialize(P.Ar);
		P.Ar << HitstopStartFrame;
		P.Ar << HitstopEndFrame;
	}

	bool ShouldReconcile(const FGSTN_CombatSyncState& AuthorityState) const
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

	/** Discrete state: interpolated proxies just take the newer frame */
	void Interpolate(const FGSTN_CombatSyncState* From, const FGSTN_CombatSyncState* To, float PCT)
	{
		*this = *To;
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
