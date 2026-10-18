// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_InputMatcher.h"
#include "Input/GSTN_InputSlot.h"

class UGSTN_CombatInputSettings;
struct FGSTN_ButtonMap;
struct FGSTN_InputHistory;

/** Context of one resolution. */
struct GSTNCOMBATCORE_API FGSTN_ResolveParams
{
	/** Tag -> bit lookup of the character's input config */
	const FGSTN_ButtonMap* Buttons = nullptr;

	/** The frame being simulated */
	int32 Now = 0;

	/** Hitstop window whose frames do not age presses (see FGSTN_MatchParams) */
	int32 PausedStart = INDEX_NONE;
	int32 PausedEnd = INDEX_NONE;
};

/** The outcome of a resolution. */
struct FGSTN_SlotResolution
{
	/** Index in the slot list of the command to execute, INDEX_NONE if none */
	int32 SlotIndex = INDEX_NONE;

	/** The winner's match (anchors, used events) */
	FGSTN_PatternMatch Match;

	/**
	 * Button event slots to consume once the command is requested: the winner's own events plus the losing presses of its
	 * press group (the buttons the settings call exclusive), so the loser does not come out a few frames later.
	 */
	TArray<uint8, TInlineAllocator<8>> EventsToConsume;

	/** Nothing fires this frame because a chord that includes the pressed button may still be completed */
	bool bDeferredForChord = false;
};

/**
 * Chooses which command a history triggers this frame (combat.md sections 8.2 and 8.3).
 *
 * Steps: every slot is matched with its own buffer rules; only the press group around the newest
 * press competes (older buffered presses lose, as a player who changed their mind expects); the competitors are ordered by
 * tier, pattern specificity and manual offset; the first that can execute wins. A slot that matches but cannot execute
 * reserves its press so lower slots cannot steal it, unless it falls through. A single-button command can wait a few
 * frames for a chord that includes its button (only for slots that enable it).
 *
 * Pure: it reads the history and writes only its result; the caller consumes the events and requests the command.
 * Deterministic for the same history, slot order and settings. The caller passes only the slots allowed right now (all
 * when the character is actionable; those a cancel or override window allows during a move). Context plays no part in
 * matching: a command is the same input everywhere, and whether it can start in this context (does it have a binding
 * whose requirements hold?) is what CanExecute answers.
 */
struct GSTNCOMBATCORE_API FGSTN_SlotResolver
{
	/**
	 * @param Slots The candidate slots, in a stable order (the index is the result and breaks ties).
	 * @param CanExecute Whether the command of slot i could start now (cost, cooldown, blocking tags...). Called at most once per slot.
	 */
	static FGSTN_SlotResolution Resolve(TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const FGSTN_InputHistory& History,
		const FGSTN_ResolveParams& Params, const UGSTN_CombatInputSettings& Settings, TFunctionRef<bool(int32 SlotIndex)> CanExecute);
};
