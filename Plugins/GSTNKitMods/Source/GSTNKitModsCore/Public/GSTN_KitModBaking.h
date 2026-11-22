// Kit mods, pure baking (combat-loadout-mods.md §8 "Baked at compile time, never evaluated in the simulation").

#pragma once

#include "CoreMinimal.h"
#include "GSTN_KitModTypes.h"

struct FGSTN_CombatTimelineData;

/**
 * The pure half of kit mods: picks the player's mod configuration for a kit, combines the modifiers that select a move, bakes a
 * derived timeline and names it. Stateless, no world, no asset load. Every machine compiles the same loadout into the same bits:
 * time is scaled in integer thousandths and rounded half up, the name is a hash of the base path and the mod ids in slot order.
 */
struct GSTNKITMODSCORE_API FGSTN_KitModBaking
{
	/** The configuration for a kit with these tags: the entry naming one of them exactly, else the wildcard (empty Kit) entry, else null */
	static const FGSTN_KitModConfig* SelectConfig(const FGSTN_LoadoutModsExt& Ext, const FGameplayTagContainer& KitTags);

	/** Multiplies the modifiers in order, unions their tags, then clamps time and reach to the kit's limits */
	static FGSTN_CombinedMoveModifier Combine(TConstArrayView<const FGSTN_MoveModifier*> Modifiers, const FGSTN_KitModLimits& Limits);

	/** Whether Filter selects a move with these tags (ability asset tags plus command). An empty filter selects every move */
	static bool MatchesFilter(const FGameplayTagQuery& Filter, const FGameplayTagContainer& MoveTags);

	/** TimeScale as integer thousandths, the only form the frame maths uses */
	static int32 QuantizeTimeScale(float TimeScale);

	/** Frame scaled by Quantized thousandths, rounded half up. Negative frames (unset markers) pass through */
	static int32 ScaleFrame(int32 Frame, int32 Quantized);

	/**
	 * Base with the modifier applied: hit data multipliers and hitbox extent on every hitbox, reach along ReachAxis (L4), then time
	 * (frames, windows of at least one frame, hitbox keys in strict order, baked sockets resampled to the new length, montage rate
	 * divided by the scale), then a Tags track for AddedTags. Recoil and landing-lag frames are not scaled: they are not the move's own
	 * time. Movement tracks get their frames scaled; their velocity curve is untouched
	 */
	static void BakeTimeline(const FGSTN_CombatTimelineData& Base, const FGSTN_CombinedMoveModifier& Modifier, FGSTN_CombatTimelineData& OutDerived);

	/** The stable object name of the derived timeline: GSTNModTL_<hash of the base path and the mod ids in slot order> */
	static FName MakeDerivedName(const FSoftObjectPath& BasePath, TConstArrayView<FSoftObjectPath> ModIdsInSlotOrder);
};
