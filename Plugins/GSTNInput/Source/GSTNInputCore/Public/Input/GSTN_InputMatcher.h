// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputPattern.h"

class UGSTN_InputSettings;
struct FGSTN_ButtonMap;

/** The buffer rules and context one match is evaluated with (the slot's overrides applied over the settings). */
struct GSTNINPUTCORE_API FGSTN_MatchParams
{
	/** Tag -> bit lookup of the character's input config */
	const FGSTN_ButtonMap* Buttons = nullptr;

	/** The frame being simulated: the command would come out now */
	int32 Now = 0;

	/** Time buffer: a terminal press is valid while its age is at most this many frames */
	int32 BufferFrames = 8;

	/** Hold buffer: also valid past the time buffer while the button has been held since the press */
	bool bHoldBuffer = true;

	/** Longest the hold buffer extends a press, in frames of age. 0 = unlimited (bounded by the history) */
	int32 MaxHoldBufferFrames = 0;

	/** Whether the terminal step's direction conditions look at the press or at the frame the command comes out */
	EGSTN_DirectionPolicy DirectionPolicy = EGSTN_DirectionPolicy::AtPress;

	/** Tolerance inside a step that does not set its own */
	int32 DefaultSimultaneityFrames = 3;

	/** Button event slots that cannot be used (bit = slot): presses reserved by a higher command that could not execute */
	uint32 ReservedEvents = 0;

	/** Frames in [PausedStart, PausedEnd) do not count towards a press's age (hitstop). INDEX_NONE = no such window */
	int32 PausedStart = INDEX_NONE;
	int32 PausedEnd = INDEX_NONE;

	/** Params with the settings' buffer defaults */
	static FGSTN_MatchParams FromSettings(const UGSTN_InputSettings& Settings, const FGSTN_ButtonMap& Buttons, int32 Now);
};

/**
 * Finds out whether a pattern is satisfied by the history right now, and which events it would use.
 *
 * Pure and deterministic: it reads the history and the params, writes only its result, allocates nothing for ordinary
 * patterns and visits the history in a fixed order. The search starts from the terminal step: its anchor candidates are
 * tried newest first and accepted by the buffer rules (age within the time buffer, or still held for the hold buffer);
 * every earlier step looks for its anchor in the MaxGapFrames before the next step's anchor, newest first, backtracking
 * when a later step cannot be completed. A button event used by a later step is not available to an earlier one.
 *
 * Bounded: at most 16 candidates per step, and the search gives up (no match, ensure in development) after 4096 condition
 * evaluations, so a malformed pattern cannot stall the simulation.
 */
struct GSTNINPUTCORE_API FGSTN_InputMatcher
{
	/** Maximum condition evaluations of one match */
	static constexpr int32 MaxEvaluations = 4096;

	/** Maximum candidates tried per step */
	static constexpr int32 MaxCandidatesPerStep = FGSTN_InputHistory::MaxEvents;

	/** @return true and the match if the pattern is satisfied */
	static bool Match(const FGSTN_InputPattern& Pattern, const FGSTN_InputHistory& History, const FGSTN_MatchParams& Params, FGSTN_PatternMatch& Out);

	/** Age in frames of an event that happened at Frame, hitstop frames excluded */
	static int32 GetAge(int32 Frame, const FGSTN_MatchParams& Params);
};
