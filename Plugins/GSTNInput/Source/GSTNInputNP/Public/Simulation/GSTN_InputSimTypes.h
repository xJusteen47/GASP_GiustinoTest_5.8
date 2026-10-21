// Network Prediction types of the input simulation (GAS-NPP fork).

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
 * Input of a model that runs the input simulation, produced once per fixed tick by the controlling machine (local player or
 * server AI). A host model uses it directly as its Network Prediction input type.
 *
 * The fork's own ability input only carries five bools per UInputAction and no history, so the model samples its
 * logical buttons itself: bit i is button i of the owner's input config, the same index on every machine. The stick is
 * sent raw, as on the controller, together with the camera yaw: the simulation derives every direction space from them
 * (stick, character-relative, world) with the facing it knows itself, so a client and the server cannot disagree on it.
 */
struct FGSTN_InputCmd
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
 * The part of a model's sync state that belongs to the input simulation; a host model embeds it as a member of its own
 * sync state (see FGSTN_CombatSyncState). It is not a sync state by itself: the host's NetSerialize, ShouldReconcile and
 * ToString call the helpers below for it.
 *
 * Requests to the ability system are stamped with the frame that made them: the ability system (which ticks right after,
 * in the same frame) executes a request only on that frame, so a re-simulation executes it again exactly once and a
 * restored older state never re-triggers it.
 *
 * Frames in here are Network Prediction frames ("server frames", the same number on every machine). The input history
 * is part of the state because it cannot be rebuilt from the commands: the server fills in missing inputs and the facing
 * comes from corrected state (combat.md section 6.4).
 */
struct FGSTN_InputSimState
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

	void NetSerialize(FArchive& Ar)
	{
		// The history writes its frames relative to the frame it was last appended at; that frame goes first
		uint32 HistoryFrame = static_cast<uint32>(History.GetLatestFrame() + 1);
		Ar.SerializeIntPacked(HistoryFrame);
		History.NetSerialize(Ar, static_cast<int32>(HistoryFrame) - 1);

		uint8 Zone = static_cast<uint8>(Classifier.Zone);
		Ar << Zone;
		Classifier.Zone = static_cast<EGSTN_StickZone>(FMath::Min<uint8>(Zone, static_cast<uint8>(EGSTN_StickZone::Tilt)));
		Ar << Classifier.LastNeutralFrame;

		Ar << WatchedTagMask;
		Ar << RequestedAction;
		Ar << RequestFrame;
		Ar << LastActivatedFrame;
	}

	/** Always returns false: the mismatches are reported through the trace macros, and the host decides what to do with them */
	bool ShouldReconcile(const FGSTN_InputSimState& AuthorityState) const
	{
		UE_NP_TRACE_RECONCILE(!(History == AuthorityState.History), "History:");
		UE_NP_TRACE_RECONCILE(!(Classifier == AuthorityState.Classifier), "Classifier:");
		UE_NP_TRACE_RECONCILE(WatchedTagMask != AuthorityState.WatchedTagMask, "WatchedTagMask:");
		UE_NP_TRACE_RECONCILE(RequestedAction != AuthorityState.RequestedAction, "RequestedAction:");
		UE_NP_TRACE_RECONCILE(RequestFrame != AuthorityState.RequestFrame, "RequestFrame:");
		UE_NP_TRACE_RECONCILE(LastActivatedFrame != AuthorityState.LastActivatedFrame, "LastActivatedFrame:");
		return false;
	}

	/** The history in the input notation: this is what np.PrintReconciles shows, which is how mismatches get diagnosed. No line break at the end */
	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("History: %s Held: 0x%08x Request: %d@%d Activated: %d Watched: 0x%llx", TCHAR_TO_ANSI(*FGSTN_InputScript::Describe(History)),
			History.GetHeldButtons(), RequestedAction, RequestFrame, LastActivatedFrame, WatchedTagMask);
	}
};
