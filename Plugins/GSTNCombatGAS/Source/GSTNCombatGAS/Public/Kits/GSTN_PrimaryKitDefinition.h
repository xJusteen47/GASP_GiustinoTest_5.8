// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Attributes/GSTN_KitStats.h"
#include "Attributes/GSTN_MeterSetup.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_CommandSet.h"
#include "Rules/GSTN_KitExtension.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_PrimaryKitDefinition.generated.h"

class UAnimInstance;
class UGameplayEffect;
class UGSTN_SuperInputScheme;
class UGSTN_WeaponDefinition;
class UTexture2D;

/**
 * A kit: the character a player picks (combat-m3 8). It carries the base stats, the commands the character understands, what
 * those commands start in each context, the meters, the supers it can use and what it may be combined with. Pure data: the
 * loadout compiles it into the binding table and applies it outside the simulation. Primary asset type GSTN_PrimaryKit.
 *
 * Every content reference is soft, so a kit can name its weapon, effects and abilities without loading any of them, and a game
 * replaces any of them by data. Behaviour an optional plugin adds (a shield's meter data, carry rules) goes in Extensions,
 * never in a new property here.
 *
 * Identity: KitTags holds exactly one leaf under the kit tag root, unique among kits; the other tags only classify.
 */
UCLASS(BlueprintType)
class GSTNCOMBATGAS_API UGSTN_PrimaryKitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * The identity leaf under the kit root, plus any classifying tags of other roots. No Categories filter: the container holds
	 * both kinds, and IsDataValid checks that exactly one tag lies under the kit root.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	FGameplayTagContainer KitTags;

	/** The weapon the kit starts with (always active at spawn) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Weapons")
	TSoftObjectPtr<UGSTN_WeaponDefinition> PrimaryWeapon;

	/** Which other weapons the kit may carry (matched against the weapons' tags). Empty query = none allowed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Weapons")
	FGameplayTagQuery AllowedAdditionalWeapons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Weapons", meta = (ClampMin = "0", ClampMax = "8"))
	int32 MaxAdditionalWeapons = 1;

	/** Base movement stats and meter maxima, fed to the init effect as set-by-caller magnitudes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Stats")
	FGSTN_KitStats Stats;

	/** Instant effect applied once at spawn with the stats as magnitudes (a meter maximum must come before its meter) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Stats")
	TSoftClassPtr<UGameplayEffect> InitStatsEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Stats")
	TArray<FGSTN_MeterSetup> Meters;

	/** The commands the kit understands: a base set minus removed plus added */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Commands")
	FGSTN_CommandSet CommandSet;

	/** What the kit's commands start, per context. The kit's own bindings come before its weapons' */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Commands")
	TArray<FGSTN_AbilityBinding> KitBindings;

	/** Granted abilities started by gameplay triggers or by other abilities, not by a command. Timeline ones enter the registry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Abilities")
	TArray<TSoftClassPtr<UGameplayAbility>> TriggeredAbilities;

	/** Abilities granted for as long as the kit is applied */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Abilities")
	TArray<TSoftClassPtr<UGameplayAbility>> Passives;

	/** Super slots the kit has */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Supers", meta = (ClampMin = "0", ClampMax = "8"))
	int32 SuperSlotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Supers")
	TSoftObjectPtr<UGSTN_SuperInputScheme> SuperInputScheme;

	/** Which supers the kit may use (matched against the supers' tags) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Supers")
	FGameplayTagQuery AllowedSupers;

	/** Which skins fit the kit (matched against the skins' tags) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Presentation")
	FGameplayTagQuery CompatibleSkins;

	/** Socket of the character that aim-origin queries start from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Presentation")
	FName AimOriginSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Presentation")
	TSoftClassPtr<UAnimInstance> LocomotionAnimLayer;

	/** Used when an attack's own timeline authors no ricochet response and its weapon has none (combat-m4) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Ricochet")
	bool bHasRicochetFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Ricochet", meta = (EditCondition = "bHasRicochetFallback"))
	FGSTN_RicochetResponse RicochetFallback;

	/** Data of optional plugins. Entries whose struct is not loaded are kept and ignored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit|Extensions")
	FGSTN_KitExtensionList Extensions;

	/** The ricochet fallback of the kit, null if none is authored */
	const FGSTN_RicochetResponse* GetRicochetFallback() const { return bHasRicochetFallback && RicochetFallback.IsAuthored() ? &RicochetFallback : nullptr; }

	/**
	 * Every check that needs no other asset: identity, stats, meters, supers, command set, bindings. Errors make the asset
	 * invalid. Loads the base command set (outside the simulation only).
	 */
	void Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	/** Validate, plus the identity unique among the registered kits */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
