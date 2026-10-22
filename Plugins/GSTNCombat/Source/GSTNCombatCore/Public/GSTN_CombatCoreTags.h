// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the combat core itself owns (modularity-and-tags 4.2): the identity roots of the data it carries and the
 * Combat.State.* leaves its state projection defines. The ability layer (GSTNCombatGAS) declares the rest of the combat
 * vocabulary in the same namespace and builds on these.
 *
 * Combat.State.* are projected by the combat tick from the simulation, never granted by an ability or an effect, so they stay
 * deterministic across a rollback. Designer tags (weapon, kit, super, loadout and skin identity) are ini content: only the
 * roots are declared so tag pickers can filter on them, and code never names a leaf.
 */
namespace GSTNCombatTags
{
	// States, projected by the combat tick

	/** The combatant is committed to an action and cannot start a new one freely. Projected, never granted. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Busy);

	/** A combat timeline is playing. Projected, never granted. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_InTimeline);

	/** The super input sequence is being entered. Projected, never granted. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SuperInput);

	/** The active timeline is inside a cancel window. Projected, never granted. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CancelWindow);

	/** The active timeline is inside its startup override window (a new command may replace it). Projected, never granted. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_OverrideWindow);

	/** The combatant is falling relative to its local gravity. Projected, never granted. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Falling);

	// Roots of the identity tags of the data the core carries: leaves are ini content, code never names one

	/** Root of weapon identity tags. Leaves are designer content. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Weapon);

	/** Root of kit identity tags. Leaves are designer content. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Kit);

	/** Root of super identity tags. Leaves are designer content. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Super);

	/** Root of loadout tags. Leaves are designer content. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Loadout);

	/** Root of skin tags. Leaves are designer content. */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Skin);

	/** Appends every tag the core declares here, so tests and validators cannot miss one. */
	GSTNCOMBATCORE_API void GetDeclaredCoreTags(TArray<FGameplayTag>& OutTags);
}
