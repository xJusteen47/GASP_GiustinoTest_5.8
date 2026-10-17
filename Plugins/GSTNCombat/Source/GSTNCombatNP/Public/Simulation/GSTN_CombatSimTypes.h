// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTrace.h"
#include "Misc/StringBuilder.h"

/**
 * Input of the combat model, produced once per fixed tick by the controlling machine (local player or server AI).
 *
 * The fork's own ability input only carries five bools per UInputAction and no history, so the combat model samples its
 * logical buttons itself: bit i is button i of the owner's button list, the same index on every machine.
 */
struct FGSTN_CombatInputCmd
{
	/** Buttons held when the input was sampled */
	uint32 HeldButtons = 0;

	/** Buttons pressed since the previous sample, so a tap shorter than one frame is not lost */
	uint32 PressedButtons = 0;

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar.SerializeIntPacked(HeldButtons);
		P.Ar.SerializeIntPacked(PressedButtons);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("Held: 0x%08x Pressed: 0x%08x\n", HeldButtons, PressedButtons);
	}
};

/**
 * State of the combat model, rolled back and re-simulated by Network Prediction.
 *
 * Requests to the ability system are stamped with the frame that made them: the ability system (which ticks right after,
 * in the same frame) executes a request only on that frame, so a re-simulation executes it again exactly once and a
 * restored older state never re-triggers it.
 */
struct FGSTN_CombatSyncState
{
	/** Held buttons of the previous frame, to detect presses */
	uint32 PrevHeldButtons = 0;

	/** Ability request for the ability system: 1-based index in the owner's request list, 0 = none */
	uint8 RequestedAction = 0;

	/** Frame that made RequestedAction */
	int32 RequestFrame = INDEX_NONE;

	/** Hitstop window [HitstopStartFrame, HitstopEndFrame) */
	int32 HitstopStartFrame = INDEX_NONE;
	int32 HitstopEndFrame = INDEX_NONE;

	bool IsInHitstop(int32 Frame) const { return Frame >= HitstopStartFrame && Frame < HitstopEndFrame; }

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar.SerializeIntPacked(PrevHeldButtons);
		P.Ar << RequestedAction;
		P.Ar << RequestFrame;
		P.Ar << HitstopStartFrame;
		P.Ar << HitstopEndFrame;
	}

	bool ShouldReconcile(const FGSTN_CombatSyncState& AuthorityState) const
	{
		UE_NP_TRACE_RECONCILE(PrevHeldButtons != AuthorityState.PrevHeldButtons, "PrevHeldButtons:");
		UE_NP_TRACE_RECONCILE(RequestedAction != AuthorityState.RequestedAction, "RequestedAction:");
		UE_NP_TRACE_RECONCILE(RequestFrame != AuthorityState.RequestFrame, "RequestFrame:");
		UE_NP_TRACE_RECONCILE(HitstopStartFrame != AuthorityState.HitstopStartFrame, "HitstopStartFrame:");
		UE_NP_TRACE_RECONCILE(HitstopEndFrame != AuthorityState.HitstopEndFrame, "HitstopEndFrame:");
		return false;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("PrevHeld: 0x%08x Request: %d@%d Hitstop: [%d, %d)\n", PrevHeldButtons, RequestedAction, RequestFrame,
			HitstopStartFrame, HitstopEndFrame);
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

using FGSTN_CombatStateTypes = TNetworkPredictionStateTypes<FGSTN_CombatInputCmd, FGSTN_CombatSyncState, FGSTN_CombatAuxState>;
