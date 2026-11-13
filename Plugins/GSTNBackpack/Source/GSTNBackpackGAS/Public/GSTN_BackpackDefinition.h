// Backpacks, ability layer (combat-loadout-mods.md §10).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "GSTN_BackpackTypes.h"
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

	/**
	 * Movement the backpack adds to the carrier's stats while carried (the jet-pack's double jump and glide, M11 L7b): applied with the
	 * Effects through UGSTN_GE_BackpackMovement. All zero = none
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Backpack|Movement")
	FGSTN_BackpackMovement Movement;

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

/**
 * Starter backpack (owner 2026-11-12): the jet-pack. A double jump (one air jump more than the kit's, on the kit's own jump command) and a
 * glide while the jump button is held after a jump. Make a data asset of this class: it is filled, no wiring. The numbers are a first guess
 * for the playtest (FGSTN_BackpackMovement)
 */
UCLASS(BlueprintType, Const)
class GSTNBACKPACKGAS_API UGSTN_BackpackDefinition_JetPack : public UGSTN_BackpackDefinition
{
	GENERATED_BODY()

public:
	UGSTN_BackpackDefinition_JetPack();
};

/**
 * Starter backpack (owner 2026-11-12): the shield dome. One move slot, Command.Backpack.Dome: Press of the layout's dodge button with the
 * stick neutral (so it beats the plain dodge only then, and a tilted dodge stays a dodge), grounded, gated by its memory slot 0
 * (Backpack.State.Empty.Slot0), running UGSTN_GA_BackpackDome. Make a data asset of this class: it is filled, no wiring
 */
UCLASS(BlueprintType, Const)
class GSTNBACKPACKGAS_API UGSTN_BackpackDefinition_ShieldDome : public UGSTN_BackpackDefinition
{
	GENERATED_BODY()

public:
	UGSTN_BackpackDefinition_ShieldDome();

	/** The dome's binding as the class default holds it (soft path to the NP module's ability: this module never links it) */
	static FGSTN_AbilityBinding MakeDomeBinding();
};
