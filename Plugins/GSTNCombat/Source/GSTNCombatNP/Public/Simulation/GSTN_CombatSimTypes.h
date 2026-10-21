// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTrace.h"
#include "Misc/StringBuilder.h"

/**
 * Input of the combat model, produced once per fixed tick by the controlling machine (local player or server AI).
 *
 * The fork's own ability input only carries five bools per UInputAction and no history, so the combat model samples its
 * logical buttons itself: bit i is button i of the owner's input config, the same index on every machine. The stick is
 * sent raw, as on the controller, together with the camera yaw: the simulation derives every direction space from them
 * (stick, character-relative, world) with the facing it knows itself, so a client and the server cannot disagree on it.
 */
struct FGSTN_CombatInputCmd
{
	/** Buttons held when the input was sampled */
	uint32 HeldButtons = 0;

	/** Buttons pressed since the previous sample, so a tap shorter than one frame is not lost */
	uint32 PressedButtons = 0;

	/** Move stick as on the controller (+Y up), clamped to the unit circle, 127 = 1 */
	int8 StickX = 0;
	int8 StickY = 0;

	/** Control rotation yaw when sampled, 256 steps per turn */
	uint8 CameraYaw = 0;

	/** Command requested by name, 0 = none; else 1 + index in the owner's command bindings. Set by AI and scripts, no input is synthesised */
	uint16 ForcedCommand = 0;

	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar.SerializeIntPacked(HeldButtons);
		P.Ar.SerializeIntPacked(PressedButtons);
		P.Ar << StickX;
		P.Ar << StickY;
		P.Ar << CameraYaw;

		uint8 bHasForcedCommand = ForcedCommand != 0 ? 1 : 0;
		P.Ar.SerializeBits(&bHasForcedCommand, 1);
		if (bHasForcedCommand)
		{
			P.Ar << ForcedCommand;
		}
		else
		{
			ForcedCommand = 0;
		}
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("Held: 0x%08x Pressed: 0x%08x Stick: (%d, %d) CameraYaw: %d Forced: %d\n", HeldButtons, PressedButtons,
			static_cast<int32>(StickX), static_cast<int32>(StickY), static_cast<int32>(CameraYaw), static_cast<int32>(ForcedCommand));
	}

	/**
	 * Network Prediction hook: the server reuses this input for a frame the client's input did not reach it in time for
	 * (gas-npp.md rule 9). The taps and the forced command already acted on the frame they came from; the held buttons,
	 * stick and camera are state and carry over, and a held button repeated does not press again (the press is the edge
	 * against the history's held mask).
	 */
	void PrepareRepeatedInput()
	{
		PressedButtons = 0;
		ForcedCommand = 0;
	}
};

/**
 * State of the combat model, rolled back and re-simulated by Network Prediction.
 *
 * Requests to the ability system are stamped with the frame that made them: the ability system (which ticks right after,
 * in the same frame) executes a request only on that frame, so a re-simulation executes it again exactly once and a
 * restored older state never re-triggers it.
 *
 * Frames in here are Network Prediction frames ("server frames", the same number on every machine). The input history
 * is part of the state because it cannot be rebuilt from the commands: the server fills in missing inputs and the facing
 * comes from corrected state (combat.md section 6.4).
 */
struct FGSTN_CombatSyncState
{
	/** Recent button and stick events, the held mask, the stick of the latest frame */
	FGSTN_InputHistory History;

	/** What the direction classifier needs from the previous frame (zone, to detect flicks) */
	FGSTN_ClassifierState Classifier;

	/** Bit i = watched tag i was on the owner at the end of the previous frame (to detect tags added this frame) */
	uint64 WatchedTagMask = 0;

	/** Ability request for the ability system: 1-based index in the owner's request list, 0 = none */
	uint8 RequestedAction = 0;

	/** Frame that made RequestedAction */
	int32 RequestFrame = INDEX_NONE;

	/** Frame of the last request made by a command, for the reset rule event "move activated" */
	int32 LastActivatedFrame = INDEX_NONE;

	/** Hitstop window [HitstopStartFrame, HitstopEndFrame) */
	int32 HitstopStartFrame = INDEX_NONE;
	int32 HitstopEndFrame = INDEX_NONE;

	bool IsInHitstop(int32 Frame) const { return Frame >= HitstopStartFrame && Frame < HitstopEndFrame; }

	void NetSerialize(const FNetSerializeParams& P)
	{
		// The history writes its frames relative to the frame it was last appended at; that frame goes first
		uint32 HistoryFrame = static_cast<uint32>(History.GetLatestFrame() + 1);
		P.Ar.SerializeIntPacked(HistoryFrame);
		History.NetSerialize(P.Ar, static_cast<int32>(HistoryFrame) - 1);

		uint8 Zone = static_cast<uint8>(Classifier.Zone);
		P.Ar << Zone;
		Classifier.Zone = static_cast<EGSTN_StickZone>(FMath::Min<uint8>(Zone, static_cast<uint8>(EGSTN_StickZone::Tilt)));
		P.Ar << Classifier.LastNeutralFrame;

		P.Ar << WatchedTagMask;
		P.Ar << RequestedAction;
		P.Ar << RequestFrame;
		P.Ar << LastActivatedFrame;
		P.Ar << HitstopStartFrame;
		P.Ar << HitstopEndFrame;
	}

	bool ShouldReconcile(const FGSTN_CombatSyncState& AuthorityState) const
	{
		UE_NP_TRACE_RECONCILE(!(History == AuthorityState.History), "History:");
		UE_NP_TRACE_RECONCILE(!(Classifier == AuthorityState.Classifier), "Classifier:");
		UE_NP_TRACE_RECONCILE(WatchedTagMask != AuthorityState.WatchedTagMask, "WatchedTagMask:");
		UE_NP_TRACE_RECONCILE(RequestedAction != AuthorityState.RequestedAction, "RequestedAction:");
		UE_NP_TRACE_RECONCILE(RequestFrame != AuthorityState.RequestFrame, "RequestFrame:");
		UE_NP_TRACE_RECONCILE(LastActivatedFrame != AuthorityState.LastActivatedFrame, "LastActivatedFrame:");
		UE_NP_TRACE_RECONCILE(HitstopStartFrame != AuthorityState.HitstopStartFrame, "HitstopStartFrame:");
		UE_NP_TRACE_RECONCILE(HitstopEndFrame != AuthorityState.HitstopEndFrame, "HitstopEndFrame:");
		return false;
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		// The history in the input notation: this is what np.PrintReconciles shows, which is how mismatches get diagnosed
		Out.Appendf("History: %s Held: 0x%08x Request: %d@%d Activated: %d Hitstop: [%d, %d) Watched: 0x%llx\n",
			TCHAR_TO_ANSI(*FGSTN_InputScript::Describe(History)), History.GetHeldButtons(), RequestedAction, RequestFrame,
			LastActivatedFrame, HitstopStartFrame, HitstopEndFrame, WatchedTagMask);
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
