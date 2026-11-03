// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Attributes/GSTN_KitStats.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Rules/GSTN_KitExtension.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_WeaponDefinition.generated.h"

class AActor;
class UAnimInstance;
class UGameplayAbility;
class UGSTN_GE_WeaponStats;

/** The numbers a weapon's stats effect reads, as set-by-caller magnitudes (same type as the kit's base stats) */
using FGSTN_WeaponStatModifiers = TArray<FGSTN_StatValue>;

/** The abilities a weapon allows in place of the one it binds to a command (a map value cannot be an array itself) */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_AbilityClassList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Alternatives")
	TArray<TSoftClassPtr<UGameplayAbility>> Abilities;
};

/**
 * A weapon: the extra commands, stats and passives that come with it while it is active (combat-m3 8). Primary asset type
 * GSTN_Weapon.
 *
 * Bindings inherit: a weapon starts from its parent's bindings and a binding of its own with the same command and the same
 * requirements replaces the parent's. The stats effect is one class per weapon, found again by class when the weapon stops
 * being active, so two weapons must not name the same class (IsDataValid checks it across the registered weapons).
 *
 * Identity: WeaponTags holds exactly one leaf under the weapon root, unique among weapons; the other tags (the weapon's role,
 * its family) only classify.
 */
UCLASS(BlueprintType)
class GSTNCOMBATGAS_API UGSTN_WeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Longest parent chain followed; a longer one is reported as a cycle */
	static constexpr int32 MaxParentDepth = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	/**
	 * The identity leaf under the weapon root, plus classifying tags of other roots (the primary or support role among them).
	 * No Categories filter: the container holds both kinds, and IsDataValid checks that exactly one tag lies under the weapon root.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FGameplayTagContainer WeaponTags;

	/** The weapon whose bindings this one starts from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UGSTN_WeaponDefinition> ParentWeapon;

	/** What the weapon's commands start, per context. Replaces or extends the parent's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Commands")
	TArray<FGSTN_AbilityBinding> Bindings;

	/** For each command, the abilities a player may pick instead of the bound one in the loadout screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Commands", meta = (ForceInlineRow))
	TMap<FGameplayTag, FGSTN_AbilityClassList> AllowedAlternatives;

	/** Infinite effect applied while the weapon is active. One class per weapon: it is removed by class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Stats")
	TSoftClassPtr<UGSTN_GE_WeaponStats> StatsEffect;

	/** The numbers the stats effect reads */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Stats")
	TArray<FGSTN_StatValue> StatModifiers;

	/** Abilities granted while the weapon is active */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Abilities")
	TArray<TSoftClassPtr<UGameplayAbility>> PassivesWhileActive;

	/** Free numbers the weapon's abilities read by tag (a reach, a charge time). The key is designer content */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Abilities", meta = (ForceInlineRow))
	TMap<FGameplayTag, float> Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
	TSoftClassPtr<UAnimInstance> AnimLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
	TSoftClassPtr<AActor> VisualActor;

	/** Used when an attack's own timeline authors no ricochet response (combat-m4); the kit's is the next fallback */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ricochet")
	bool bHasRicochetFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ricochet", meta = (EditCondition = "bHasRicochetFallback"))
	FGSTN_RicochetResponse RicochetFallback;

	/** Data of optional plugins. Entries whose struct is not loaded are kept and ignored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Extensions")
	FGSTN_KitExtensionList Extensions;

	/** The ricochet fallback of the weapon, null if none is authored */
	const FGSTN_RicochetResponse* GetRicochetFallback() const { return bHasRicochetFallback && RicochetFallback.IsAuthored() ? &RicochetFallback : nullptr; }

	/**
	 * The parent chain, root first, this weapon last. Loads the parents (outside the simulation only). bOutBroken is set when
	 * the chain loops, is deeper than MaxParentDepth or names a parent that does not load; the chain then stops there.
	 */
	void GetParentChain(TArray<const UGSTN_WeaponDefinition*>& OutChain, bool& bOutBroken) const;

	/** The bindings after inheritance along the whole parent chain */
	void GetEffectiveBindings(TArray<FGSTN_AbilityBinding>& OutBindings) const;

	/** Every check that needs no other asset. Errors make the asset invalid. Loads the parents. */
	void Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	/** Validate, plus the identity and the stats effect class unique among the registered weapons */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
