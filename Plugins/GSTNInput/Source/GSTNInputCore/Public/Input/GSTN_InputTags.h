// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the input plugin itself produces or reads (input.md section 3). Buttons, commands and the lock tags of
 * each command root are game data: the game declares them, natively or in Config/Tags/GSTNCommands.ini.
 */
namespace GSTNInputTags
{
	// Roots, for the tag pickers

	/** Root of the logical button tags */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button);

	/** Root of the command tags; CommandTiers and the lock mapping work on its children */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command);

	/** Locks every command; Input.Lock.Command.<root> locks Command.<root>.* */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lock_Command);

	// Context tags the host projects from the owner's and Mover's state each frame, for bindings to require

	/** The character is on the ground */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_Grounded);

	/** The character is in the air (falling or rising, not on a wall) */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_Airborne);

	/** The character is in a wall movement mode; with Airborne it identifies wall bindings */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_OnWall);

	// Frame events, for buffer reset rules. Other systems declare theirs under the same root.

	/** Root of the frame events reset rules listen to */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BufferEvent);

	/** An ability started from a command this frame */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BufferEvent_AbilityActivated);

	/** The character landed this frame */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BufferEvent_Landed);

	/** The character was interrupted by something outside its control (a hit received, a stun, a grab). Raised by whichever system interrupts it; a reset rule decides what the buffer forgets */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BufferEvent_Interrupted);

	// Authoritative events

	/** A command was executed (published through the authoritative event stream) */
	GSTNINPUTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_CommandExecuted);
}
