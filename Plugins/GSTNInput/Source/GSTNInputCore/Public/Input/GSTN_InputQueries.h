// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_InputHistory.h"

/**
 * Questions a move asks about the recent input (CP1 of the Blueprint seams of the combat plugin): "was this button pressed in the last N
 * frames", "how many times", "how long has it been held". The NP adapter answers them from the rolled-back input history of the frame being
 * simulated, by button tag, so a Blueprint names a button and never a key (no hardcoded input); this is the arithmetic on the history.
 *
 * Pure, deterministic and allocation free: they read the history and their arguments and nothing else, so the answer is the same on every
 * machine and in every re-simulation. Reach: the history keeps the newest FGSTN_InputHistory::MaxButtonEvents button events and nothing
 * older than MaxEventAge frames, so a question about a long window can miss a press that already fell out (ask about short windows: a
 * double press, a mash count in the last second).
 *
 * Ages count combat frames like the input buffer (FGSTN_InputMatcher::GetAge): the frames of the hitstop window in FAgeContext do not count,
 * so a press made just before a hit is as fresh after the hitstop as it was before it.
 */
struct GSTNINPUTCORE_API FGSTN_InputQueries
{
	/** The frame the question is asked on and the hitstop window whose frames do not age a press */
	struct FAgeContext
	{
		/** The frame being simulated (the history's latest frame) */
		int32 Now = 0;

		/** Hitstop window [PausedStart, PausedEnd) in simulation frames; INDEX_NONE = none */
		int32 PausedStart = INDEX_NONE;
		int32 PausedEnd = INDEX_NONE;
	};

	/**
	 * Age in frames of the newest press of Button made at or before Now (0 = pressed on this very frame), INDEX_NONE when the history holds
	 * none. bIncludeUsed: a press a command already used (consumed) counts too; false asks only about presses still waiting in the buffer
	 */
	static int32 GetNewestPressAge(const FGSTN_InputHistory& History, int32 Button, const FAgeContext& Context, bool bIncludeUsed);

	/** A press of Button whose age is at most Frames (Frames 0 = on this frame only) */
	static bool WasPressedWithin(const FGSTN_InputHistory& History, int32 Button, const FAgeContext& Context, int32 Frames, bool bIncludeUsed)
	{
		const int32 Age = GetNewestPressAge(History, Button, Context, bIncludeUsed);
		return Frames >= 0 && Age != INDEX_NONE && Age <= Frames;
	}

	/** How many presses of Button have an age of at most Frames (a double press is 2) */
	static int32 CountPressesWithin(const FGSTN_InputHistory& History, int32 Button, const FAgeContext& Context, int32 Frames, bool bIncludeUsed);

	/**
	 * Frames Button has been down at Now (0 on the frame of its press), INDEX_NONE when it is up. A tap shorter than a frame is never down
	 * (FGSTN_InputHistory::IsButtonHeldAt). MAX_int32 when the press is older than the history. Raw frames: a hold keeps growing in hitstop,
	 * like the Hold conditions of the patterns
	 */
	static int32 GetHeldFrames(const FGSTN_InputHistory& History, int32 Button, int32 Now);

	/** Whether Button is a bit the history can hold */
	static bool IsValidButton(int32 Button) { return Button >= 0 && Button < 32; }
};
