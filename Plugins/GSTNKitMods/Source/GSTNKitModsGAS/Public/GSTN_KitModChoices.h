// Kit mods, ability layer: the mod rows of the loadout screen.

#pragma once

#include "CoreMinimal.h"

struct FGSTN_LoadoutChoiceContext;
struct FGSTN_LoadoutChoiceRow;

/**
 * One row per mod slot of the draft's kit (FGSTN_KitModSlots): "none" and every kit mod asset of that slot type that fits the kit
 * (CompatibleKits on the kit's and primary weapon's tags). A pick writes the player's configuration for that kit (FGSTN_KitModConfig
 * keyed by the kit's first KitTags tag, a wildcard entry when the kit has no tag): mods are chosen per kit, before the match, and a kit
 * without slots shows no row. Registered from the module startup with FGSTN_LoadoutChoiceRegistry.
 */
struct GSTNKITMODSGAS_API FGSTN_KitModChoices
{
	static const FName ProviderId;
	static constexpr int32 ProviderPriority = 200;

	static void Gather(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows);
};
