// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "Display/GSTN_InputDisplayLayout.h"

class UGSTN_InputSettings;

/** What a typed notation does: the history it plays and the command the slots resolve on its last frame */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayScriptResult
{
	/** False when the notation does not parse or is an impossible sequence; Error says why and Data is empty */
	bool bOk = false;
	FString Error;

	/** The scripted history. The press the command used is already consumed (greyed on the strip) and the command is a marker on the last frame */
	FGSTN_InputDisplayData Data;

	int32 LastFrame = INDEX_NONE;

	/** The command resolved at LastFrame, invalid when none matches */
	FGameplayTag Command;

	/** Index in the slots, INDEX_NONE when none matches */
	int32 SlotIndex = INDEX_NONE;
};

/**
 * "What does this input do": plays a notation of FGSTN_InputScript into a history and asks the slot resolver which of the given
 * slots comes out on the last frame, exactly as the simulation would (the real classifier, matcher and priority; every command
 * counts as executable, because whether an ability can start is a matter of the game state, not of the input). The slots are a
 * command set that the source of the history hands over (the combat adapter: the plugin's standard set; a kit's composed set once something passes it on).
 *
 * Pure: it builds a history and reads the settings, no world, no ability system.
 */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayScript
{
	static FGSTN_InputDisplayScriptResult Resolve(const FString& Notation, TConstArrayView<FGameplayTag> ButtonTags,
		TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const UGSTN_InputSettings& Settings);
};
