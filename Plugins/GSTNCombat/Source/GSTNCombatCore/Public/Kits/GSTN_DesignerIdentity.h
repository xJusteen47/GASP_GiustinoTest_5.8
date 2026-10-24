// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/PrimaryAssetId.h"
#include "GSTN_DesignerIdentity.generated.h"

/** One asset and the key it must not share with another asset of its type (an identity tag, a stats effect class...) */
struct GSTNCOMBATCORE_API FGSTN_UniqueKeyEntry
{
	FName Asset;
	FName Key;
};

/**
 * The identity rules of designer-made assets (modularity-and-tags 4.2b). A kit or weapon is identified by exactly one leaf
 * under the kit or weapon root; every other tag on the asset only classifies it. The leaves are designer content in a tag
 * ini and code never names one: the root is a parameter here, supplied by the module that declares it.
 *
 * Pure over tag containers and lists, so the rules are testable without assets.
 */
struct GSTNCOMBATCORE_API FGSTN_DesignerIdentity
{
	/** The tags of Tags strictly below Root (Root itself is not a leaf) */
	static void GetIdentityLeaves(const FGameplayTagContainer& Tags, const FGameplayTag& Root, TArray<FGameplayTag>& OutLeaves);

	/**
	 * True when Tags holds exactly one identity leaf under Root and not Root itself. Otherwise adds one error describing what
	 * was found (none, several, or the bare root). What names the asset kind in the message ("kit", "weapon").
	 */
	static bool ValidateExactlyOne(const FGameplayTagContainer& Tags, const FGameplayTag& Root, const FString& What, TArray<FText>& OutErrors);

	/** Adds one error per key shared by two or more assets (listing them). Entries with no key are ignored. True when none is shared. */
	static bool FindDuplicateKeys(TConstArrayView<FGSTN_UniqueKeyEntry> Entries, const FString& What, TArray<FText>& OutErrors);

#if WITH_EDITOR
	/**
	 * Appends the key of every other registered primary asset of Type (assets are loaded to read their key; Self is skipped).
	 * Does nothing when the asset manager is not up or knows no asset of that type, which is the case until the project
	 * registers the type. Editor validation only.
	 */
	static void GatherFromPrimaryAssets(FPrimaryAssetType Type, const UObject& Self, TFunctionRef<FName(const UObject&)> GetKey, TArray<FGSTN_UniqueKeyEntry>& OutEntries);
#endif
};

/** Blueprint-facing readers of the identity rule. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_DesignerIdentityUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The single identity leaf of Tags under Root; false (and an empty tag) when there is none or more than one. */
	UFUNCTION(BlueprintPure, Category = "Combat|Identity")
	static bool FindIdentityTag(const FGameplayTagContainer& Tags, FGameplayTag Root, FGameplayTag& OutIdentity);
};
