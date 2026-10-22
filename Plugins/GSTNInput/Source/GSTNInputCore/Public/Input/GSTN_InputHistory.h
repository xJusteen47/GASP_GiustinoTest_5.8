// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_InputSample.h"

/** A button going down or up. Presses are consumed by activations and reset rules, releases never are. */
struct FGSTN_ButtonEvent
{
	int32 Frame = INDEX_NONE;

	/** Index (= bit) of the button in the input config */
	uint8 Button : 5 = 0;

	uint8 bRelease : 1 = 0;

	/** A press already used by a command (or flushed by a reset rule): neither the time nor the hold buffer can use it */
	uint8 bConsumed : 1 = 0;

	bool operator==(const FGSTN_ButtonEvent& Other) const
	{
		return Frame == Other.Frame && Button == Other.Button && bRelease == Other.bRelease && bConsumed == Other.bConsumed;
	}
};

/** The stick changing direction in any space, or zone, or flicking: the stick state from this frame on. */
struct FGSTN_DirectionEvent
{
	int32 Frame = INDEX_NONE;
	FGSTN_StickSample Stick;

	bool operator==(const FGSTN_DirectionEvent& Other) const
	{
		return Frame == Other.Frame && Stick == Other.Stick;
	}
};

/**
 * Compact, rolled-back log of recent button and stick events (combat.md section 6.4). It is part of the host sync
 * state: the pattern matcher reads it, the host tick appends one sample per frame, the resolver consumes presses in it.
 *
 * Why a log in the state and not a function of the inputs: after a correction the server fills missing inputs and the
 * facing comes from corrected state, so the history cannot be rebuilt from commands alone.
 *
 * Determinism and size rules:
 *  - Plain data, copied every frame: two rings of 16 events (fixed storage, no allocation), the held mask and the stick
 *    of the newest frame. Ring overflow drops the oldest event.
 *  - Events older than MaxEventAge frames are dropped, so a state that did not change for a while does not change at all
 *    (cheap delta serialization) and serializing with 8-bit frame deltas loses nothing. The one exception is the newest
 *    stick event: it is the current stick state, so it stays and its frame becomes AncientFrame.
 *  - A new stick event is created only when a direction (in any space) or the zone changes, or on a flick; so turning the
 *    character or the camera with the stick held adds at most one event per 45 degree sector.
 *  - The frame of the last consumed stick anchor travels with the history (one optional byte on the wire, absent while
 *    nothing is consumed or once it is older than MaxEventAge): it is what stops a stick-only command from re-firing.
 *  - Equality is logical (same events in the same order), not bitwise: the same history can sit at different positions
 *    of the rings.
 *
 * "Event slot" below is the physical position (0..15) of an event in its ring. A slot is stable while the event lives
 * and is what the matcher reports as used events and what ReservedEvents masks refer to.
 */
struct GSTNINPUTCORE_API FGSTN_InputHistory
{
	/** Capacity of each ring */
	static constexpr int32 MaxEvents = 16;

	/** Events older than this many frames are dropped */
	static constexpr int32 MaxEventAge = 254;

	/** Frame of the newest stick event once it is older than MaxEventAge: before any frame that can be asked about */
	static constexpr int32 AncientFrame = MIN_int32 / 2;

	/** Appends the events of one frame (press events in button order, then releases, then the stick if it changed) */
	void Append(const FGSTN_InputSample& Sample);

	/** Frame of the last appended sample, INDEX_NONE before the first */
	int32 GetLatestFrame() const { return Current.Frame; }

	/** Buttons held at the latest frame */
	uint32 GetHeldButtons() const { return HeldButtons; }

	// ---- Button events

	int32 GetNumButtonEvents() const { return ButtonCount; }

	/** Slot of the event of the given age (0 = newest) */
	int32 GetButtonSlot(int32 Age) const
	{
		check(Age >= 0 && Age < ButtonCount);
		return (ButtonHead - 1 - Age) & (MaxEvents - 1);
	}

	const FGSTN_ButtonEvent& GetButtonEvent(int32 Slot) const { return ButtonEvents[Slot]; }

	/** Marks a press as used. Ends its time and hold buffering */
	void Consume(int32 Slot);

	/** Consumes every unconsumed press of the buttons in the mask made at or before UpToFrame (reset rules) */
	void FlushPresses(uint32 ButtonMask, int32 UpToFrame);

	/**
	 * Marks the stick anchors up to Frame as used. A command that ends in a stick pattern (a motion, a spin) has no press to
	 * consume, so without this it would fire again on every frame its anchor stays inside the time buffer. The matcher
	 * ignores terminal stick anchors at or before this frame; a fresh motion has a later anchor and fires again.
	 */
	void ConsumeSticksUpTo(int32 Frame) { LastConsumedStickFrame = FMath::Max(LastConsumedStickFrame, Frame); }

	/** Newest stick anchor frame already used by a command; AncientFrame when none (or when it aged out of the history) */
	int32 GetLastConsumedStickFrame() const { return LastConsumedStickFrame; }

	/**
	 * True if the button is down now and was not released since the press at PressFrame: the press is still being held,
	 * which keeps it valid for the hold buffer.
	 */
	bool IsHeldContinuouslySince(int32 Button, int32 PressFrame) const;

	/**
	 * Whether the button is down during Frame, and how long it had been (Frame minus the frame of its press). A button is
	 * down from its press frame up to, not including, its release frame, so a tap is never down. OutHeldFrames is MAX_int32
	 * when the press is older than the history. Used by Hold conditions.
	 */
	bool IsButtonHeldAt(int32 Button, int32 Frame, int32& OutHeldFrames) const;

	// ---- Stick events

	int32 GetNumDirectionEvents() const { return DirectionCount; }

	/** Stick event of the given age (0 = newest) */
	const FGSTN_DirectionEvent& GetDirectionEvent(int32 Age) const
	{
		check(Age >= 0 && Age < DirectionCount);
		return DirectionEvents[(DirectionHead - 1 - Age) & (MaxEvents - 1)];
	}

	/**
	 * The stick at a frame: the last stick event at or before it (the stick keeps its state between events). The flick
	 * flag is only set on the frame it happened. Neutral before the first event.
	 */
	FGSTN_StickSample GetStickAt(int32 Frame) const;

	EGSTN_StickDirection GetDirectionAt(int32 Frame, EGSTN_DirectionSpace Space) const
	{
		return GetStickAt(Frame).GetDirection(Space);
	}

	/** Stick state of the latest frame (also when it did not change) */
	const FGSTN_StickSample& GetCurrentStick() const { return Current.Stick; }

	// ---- Replication

	/**
	 * Saves or loads the history. Frames are written as 8-bit deltas from CurrentFrame (the frame the owner is
	 * serializing for), the stick state of the latest frame is rebuilt from the newest stick event. Idle history: 3 bytes
	 * plus the held mask. Loading validates the data and sets the archive error on garbage.
	 */
	void NetSerialize(FArchive& Ar, int32 CurrentFrame);

	/** Logical equality (see class comment) */
	bool operator==(const FGSTN_InputHistory& Other) const;

private:
	void PushButtonEvent(int32 Frame, int32 Button, bool bRelease);
	void PushDirectionEvent(const FGSTN_DirectionEvent& Event);
	void DropOldEvents(int32 Now);

	TStaticArray<FGSTN_ButtonEvent, MaxEvents> ButtonEvents;

	/** Next slot to write; with a full ring it is also the oldest event */
	uint8 ButtonHead = 0;
	uint8 ButtonCount = 0;

	TStaticArray<FGSTN_DirectionEvent, MaxEvents> DirectionEvents;
	uint8 DirectionHead = 0;
	uint8 DirectionCount = 0;

	uint32 HeldButtons = 0;

	/** See ConsumeSticksUpTo. Ages like an event: past MaxEventAge it goes back to AncientFrame, so an idle history stays unchanged */
	int32 LastConsumedStickFrame = AncientFrame;

	/** Stick of the newest frame. Its directions always equal the newest stick event's (or Neutral when there is none) */
	FGSTN_DirectionEvent Current;
};
