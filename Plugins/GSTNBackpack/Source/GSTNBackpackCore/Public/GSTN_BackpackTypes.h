// Backpacks, pure data (combat-loadout-mods.md §10). No ability system, no world.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Loadout/GSTN_LoadoutExtensionData.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_BackpackTypes.generated.h"

namespace GSTNBackpackTags
{
	/** Root of the backpack identity tags (Backpack.<Name>); designer leaves live in ini */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Backpack);

	/**
	 * The logical command of a backpack's main verb (spec §10). Only the command is native: which button enters it is data (the
	 * backpack binding's slot, or the kit's command set), never fixed here (question M11.11)
	 */
	GSTNBACKPACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Backpack_Use);
}

/**
 * The backpack in a player's loadout (spec §11.1, wire id "Backpack"). One backpack at a time; ids only, because the loadout travels to
 * the server and into the save game. Kept through a whole-kit swap by default (spec §10: "a kit change keeps the backpack"); a game that
 * wants the backpack to travel with the kit sets bKeepOnKitSwap false in the loadout it hands out.
 */
USTRUCT(BlueprintType)
struct GSTNBACKPACKCORE_API FGSTN_LoadoutBackpackExt : public FGSTN_LoadoutExtensionData
{
	GENERATED_BODY()

	static const FName WireId;

	/** The backpack asset (UGSTN_BackpackDefinition). Empty = no backpack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack", meta = (AllowedClasses = "/Script/GSTNBackpackGAS.GSTN_BackpackDefinition"))
	FSoftObjectPath Backpack;

	/** Stays with the player when they take a whole kit from the ground (true), or goes down with the kit they leave (false) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack")
	bool bKeepOnKitSwap = true;

	virtual bool StaysWithPlayerOnKitSwap() const override { return bKeepOnKitSwap && Backpack.IsValid(); }
};

/** The pure backpack rules on a loadout; the same answer on every machine */
struct GSTNBACKPACKCORE_API FGSTN_BackpackLoadoutRules
{
	/** The backpack the loadout carries, empty when none */
	static FSoftObjectPath GetBackpack(const FGSTN_LoadoutData& Loadout);

	/**
	 * Puts Backpack on (spec §10: one at a time, picking up another drops the current one). OutNew is Current with the new backpack,
	 * OutPrevious the one it replaces (empty when none), which the caller drops. False with a reason for an empty backpack or the one
	 * already carried. The other loadout fields and extensions are untouched
	 */
	static bool Equip(const FGSTN_LoadoutData& Current, const FSoftObjectPath& Backpack, FGSTN_LoadoutData& OutNew, FSoftObjectPath& OutPrevious, TArray<FString>& OutErrors);

	/** Takes the backpack off (a drop). OutRemoved is what it carried, empty when nothing */
	static void Remove(const FGSTN_LoadoutData& Current, FGSTN_LoadoutData& OutNew, FSoftObjectPath& OutRemoved);
};
