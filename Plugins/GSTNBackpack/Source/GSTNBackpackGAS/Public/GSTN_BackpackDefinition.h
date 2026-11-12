// Backpacks, ability layer (combat-loadout-mods.md §10).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "GSTN_BackpackDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * One backpack (spec §10, Helldivers style): a data asset the player carries on top of the kit's back implant. Everything it does is
 * an ability (a jet-pack burst, a shield dome, a supply drop): its bindings join the kit's when the loadout is compiled, so they stay
 * bound whatever weapon is active, and the rollback tick runs them like any kit move. Nothing here runs code of its own.
 */
UCLASS(BlueprintType, Const)
class GSTNBACKPACKGAS_API UGSTN_BackpackDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	/** Identity: exactly one leaf under Backpack (Backpack.JetPack); other tags only classify */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack", meta = (Categories = "Backpack"))
	FGameplayTagContainer BackpackTags;

	/**
	 * The backpack's look. A part with neither Socket nor ParentSlot goes on the kit implant's connector (Implant.Back,
	 * Connector_Backpack), so any backpack fits any kit; with no implant it sits on the back socket
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack|Visual")
	FGSTN_EquipmentVisual Visual;

	/**
	 * The verbs, as kit bindings (usually one on Command.Backpack.Use). The button is the binding's slot or the kit's command set:
	 * data, never fixed by this plugin
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack|Abilities")
	TArray<FGSTN_AbilityBinding> Abilities;

	/** Passive abilities granted while the backpack is carried */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack|Abilities")
	TArray<TSoftClassPtr<UGameplayAbility>> Passives;

	/** Effects applied on the authority while the backpack is carried (weight, a stat), removed when the loadout is cleared */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack|Abilities")
	TArray<TSubclassOf<UGameplayEffect>> Effects;

	/**
	 * The support weapons this backpack feeds (a team reload pack), matched against the additional weapons' tags. Empty = it feeds
	 * none. Carrying it without such a weapon is allowed; the rules view only notes it
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack|Support")
	FGameplayTagQuery CompatibleSupportWeapons;

	/** Dropped as a pickup when the carrier dies. Data only until the drop hook is called (the kit's DropKit is not called yet either) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack")
	bool bDropOnDeath = true;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
