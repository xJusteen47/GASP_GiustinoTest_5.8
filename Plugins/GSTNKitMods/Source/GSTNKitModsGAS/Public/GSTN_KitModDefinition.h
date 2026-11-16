// Kit mods, ability layer (combat-loadout-mods.md §8).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_KitModTypes.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "GSTN_KitModDefinition.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * One kit mod (spec §8): a data asset a player puts in a kit's mod slot. Everything here is read when the loadout is compiled or
 * granted, outside the simulation: move modifiers become derived timelines, verbs become bindings, stats become one effect. A mod
 * never runs code of its own in the rollback tick.
 */
UCLASS(BlueprintType, Const)
class GSTNKITMODSGAS_API UGSTN_KitModDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	/** Identity: exactly one leaf under Mod (Mod.LongBlade); other tags only classify */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod", meta = (Categories = "Mod"))
	FGameplayTagContainer ModTags;

	/** The slot type the mod fits (Mod.Slot.Blade...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod", meta = (Categories = "Mod.Slot"))
	FGameplayTag Slot;

	/** Matched against the kit's tags plus its primary weapon's tags. Empty = every kit with a fitting slot */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod")
	FGameplayTagQuery CompatibleKits;

	/** Changes to the moves its filters select, applied in slot order with the other mods' */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Moves")
	TArray<FGSTN_MoveModifier> MoveModifiers;

	/** Stat change: applied on the authority when the loadout is granted, removed when it is cleared */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Stats")
	TSubclassOf<UGameplayEffect> StatsEffect;

	/** SetByCaller magnitudes of StatsEffect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Stats", meta = (ForceInlineRow))
	TMap<FGameplayTag, float> StatsMagnitudes;

	/** Extra special alternatives per command, added to what the primary weapon allows (M3 §8) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Specials", meta = (ForceInlineRow, Categories = "Command"))
	TMap<FGameplayTag, FGSTN_AbilityClassList> UnlockedAlternatives;

	/** Verbs the mod adds, as kit bindings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Verbs")
	TArray<FGSTN_AbilityBinding> AddedBindings;

	/** Commands whose bindings the mod removes, from the kit and every weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Verbs", meta = (Categories = "Command"))
	FGameplayTagContainer RemovedCommands;

	/** Passive abilities granted while the mod is equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Verbs")
	TArray<TSoftClassPtr<UGameplayAbility>> Passives;

	/** The mod's look (L6 requests: attachments on the implant or weapon slots, mesh swaps, material parameters). Data only for now */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mod|Visual")
	FGSTN_EquipmentVisual Visual;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
