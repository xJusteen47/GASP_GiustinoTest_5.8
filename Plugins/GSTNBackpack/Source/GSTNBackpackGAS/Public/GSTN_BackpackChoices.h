// Backpacks, ability layer: the backpack row of the loadout screen.

#pragma once

#include "CoreMinimal.h"

struct FGSTN_LoadoutChoiceContext;
struct FGSTN_LoadoutChoiceRow;

/**
 * The backpack row of the loadout screen: "none" and every backpack asset (UGSTN_BackpackDefinition). Any backpack fits any kit (it
 * sits on the implant's connector), so the row is there whenever a backpack exists; the loadout rules still judge each option. A pick
 * goes through FGSTN_BackpackLoadoutRules (one backpack at a time). Registered from the module startup with FGSTN_LoadoutChoiceRegistry.
 */
struct GSTNBACKPACKGAS_API FGSTN_BackpackChoices
{
	static const FName ProviderId;
	static constexpr int32 ProviderPriority = 300;

	static void Gather(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows);
};
