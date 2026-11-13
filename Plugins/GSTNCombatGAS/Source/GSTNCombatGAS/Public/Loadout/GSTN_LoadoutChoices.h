// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Templates/Function.h"
#include "UObject/PrimaryAssetId.h"
#include "GSTN_LoadoutChoices.generated.h"

struct FGSTN_ResolvedLoadout;

/**
 * One value a row of the loadout screen can take. It carries the whole loadout that picking it gives (Result), not an edit, so a
 * screen never needs to know what a row means: it shows Label and, on a pick, makes Result its draft.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_LoadoutChoiceOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FText Label;

	/** The draft with this value picked */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FGSTN_LoadoutData Result;

	/** Whether the server's rules would accept Result (filled when the rows are gathered with validation) */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	bool bValid = true;

	/** The first rule finding when not valid, ready to show */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FString Problem;
};

/** One line of the loadout screen: what it changes (kit, a special, a mod slot, the backpack...) and the values it offers */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_LoadoutChoiceRow
{
	GENERATED_BODY()

	/** Stable id of the row ("Kit", "Special.Command.Special.Side", "Mod.0"...): the screen keeps its cursor on it across a rebuild */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	TArray<FGSTN_LoadoutChoiceOption> Options;

	/** The option the draft holds now, INDEX_NONE when none matches (a hand-made loadout) */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	int32 Selected = INDEX_NONE;
};

/**
 * What a row provider is handed: the draft, its assets resolved once, and the content lists of this machine. The lists are functions
 * so a test answers them from memory and a game with its own content source replaces them (UGSTN_LoadoutComponent's virtuals).
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutChoiceContext
{
	const FGSTN_LoadoutData& Draft;
	const FGSTN_ResolvedLoadout& Resolved;

	/** Every primary asset id of a type the Asset Manager knows (kits, weapons...) */
	TFunctionRef<void(const FPrimaryAssetType&, TArray<FPrimaryAssetId>&)> ListAssetIds;

	/** The asset an id names, loaded (null when unknown) */
	TFunctionRef<UObject*(const FPrimaryAssetId&)> ResolveAsset;

	/** Every asset of a class (and its children), for content not scanned by the Asset Manager (mods, backpacks) */
	TFunctionRef<void(const UClass*, TArray<FSoftObjectPath>&)> ListAssetsOfClass;
};

/** Adds the rows of one plugin for a draft. Game thread, outside the simulation; called again after every pick */
using FGSTN_LoadoutChoiceProvider = TFunction<void(const FGSTN_LoadoutChoiceContext&, TArray<FGSTN_LoadoutChoiceRow>&)>;

/**
 * The rows of the loadout screen (M11 loadout screen). The combat layer gives the kit row and one row per special that has alternatives;
 * an optional plugin registers its own rows from its module startup (mods: GSTNKitModsGAS, backpacks: GSTNBackpackGAS) and removes
 * them at shutdown, so a disabled plugin is simply absent from the screen. Providers run by priority, then id: the built-in rows sit
 * at 0 (kit) and 100 (specials). Rows with no option are dropped. Game thread only.
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutChoiceRegistry
{
	static void Register(FName Id, int32 Priority, FGSTN_LoadoutChoiceProvider Provider);
	static bool Unregister(FName Id);
	static bool IsRegistered(FName Id);

	/** The built-in rows, then every registered provider, in order. Options are not validated here (UGSTN_LoadoutComponent::GatherChoices does it) */
	static void Gather(const FGSTN_LoadoutChoiceContext& Context, TArray<FGSTN_LoadoutChoiceRow>& OutRows);

	/** Short readable name of an asset or class path: the object name without a type prefix (KIT_, WPN_, MOD_, BP_, GA_...) nor the _C suffix */
	static FText PrettyName(const FSoftObjectPath& Path);

	/** The same for a bare object or asset name */
	static FText PrettyNameFromString(FString Name);

	/** Index of the option whose Result equals Draft, else INDEX_NONE */
	static int32 FindSelected(const FGSTN_LoadoutChoiceRow& Row, const FGSTN_LoadoutData& Draft);
};
