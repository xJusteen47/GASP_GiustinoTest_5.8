// Interaction focus tags (interaction.md §8). No GAS, no Network Prediction, no Mover.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the Interact root (owner GSTNInteract). Only the roots are native here: every Interact.Target.<Meaning>
 * leaf is declared by the plugin that adds the meaning (Grab, Mount, Pickup, Equipment, Objective) or by the content's own
 * ini (Chest, Talk, Lever), so code never names a leaf and the subtree stays open. Projected tags follow combat.md 5.4.
 */
namespace GSTNInteractTags
{
	/** Root of every Interact tag. */
	GSTNINTERACTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Interact);

	/** Root of the meanings the focus can project (exactly one Interact.Target.* leaf at a time). Pickers use Categories="Interact.Target" */
	GSTNINTERACTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Target);

	/** Local presentation event raised when the presented focus changes (audio, UI). Never inside the simulation, never replicated */
	GSTNINTERACTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_FocusChanged);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNINTERACTCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
