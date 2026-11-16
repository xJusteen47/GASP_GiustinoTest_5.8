// Kit mods, content helpers for setup scripts and the test hub (M11 L10).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "GSTN_KitModContentLibrary.generated.h"

class UGSTN_KitModDefinition;
class UGSTN_PrimaryKitDefinition;

/**
 * What an editor script needs to make mod content without hand edits (Tools/GAS/setup_m11_ranger_content.py): the struct-heavy parts
 * (instanced kit and loadout extensions, move modifiers) are written here, where the types are known, so Python never builds an
 * FInstancedStruct. Nothing here runs in the simulation: kits and mods are read at compile time, a loadout is data until it is applied.
 */
UCLASS()
class GSTNKITMODSGAS_API UGSTN_KitModContentLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gives Kit a Kit mod slots extension with Slots (replacing the one it has) and the reach limit. A kit without it takes no mod.
	 * Returns whether the kit changed (dirty only then)
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|KitMods|Content")
	static bool SetKitModSlots(UGSTN_PrimaryKitDefinition* Kit, const TArray<FGameplayTag>& Slots, float MaxReachScale = 2.f);

	/**
	 * Writes a one-modifier mod for every move of the kit and its weapons (empty filter): identity tag, slot and the scales. The other
	 * fields of the asset (stats, verbs, visual) are left as they are. Returns whether the mod changed
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN|KitMods|Content")
	static bool WriteAllMovesMod(UGSTN_KitModDefinition* Mod, FGameplayTag ModTag, FGameplayTag Slot, float TimeScale, float HitboxScale,
		float ReachScale, float DamageScale, float KnockbackScale);

	/**
	 * Loadout with Mods in a wildcard config (any kit), replacing the loadout's mods extension. A kit pickup carrying it hands the mods to
	 * whoever takes it (KitSwapMods = GroundKit, the default rule). ModPaths are object paths of UGSTN_KitModDefinition assets as text
	 * (/Game/Dir/MOD_X.MOD_X), so a script passes plain strings; an empty or unparsable entry is skipped
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|KitMods|Content")
	static FGSTN_LoadoutData WithKitMods(const FGSTN_LoadoutData& Loadout, const TArray<FString>& ModPaths);

	/** The mod object paths (as text) of the loadout's wildcard config, empty without mods: what a pickup or a player carries */
	UFUNCTION(BlueprintPure, Category = "GSTN|KitMods|Content")
	static TArray<FString> GetAnyKitMods(const FGSTN_LoadoutData& Loadout);
};
