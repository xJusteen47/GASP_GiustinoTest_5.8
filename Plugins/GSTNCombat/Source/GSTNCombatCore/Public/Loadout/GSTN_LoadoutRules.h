// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "GSTN_LoadoutRules.generated.h"

/** The abilities a weapon allows in place of the one it binds to a command */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_AllowedAlternativeView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** Opaque keys of the ability classes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FSoftObjectPath> Abilities;
};

/** What the rules see of the kit */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutKitView
{
	GENERATED_BODY()

	/** The loadout's kit id resolved to an asset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	bool bFound = false;

	/** Weapons the kit may carry besides its primary one, matched against the weapons' tags. An empty query allows none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagQuery AllowedAdditionalWeapons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (ClampMin = "0"))
	int32 MaxAdditionalWeapons = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (ClampMin = "0"))
	int32 SuperSlotCount = 0;

	/** Supers the kit may use, matched against the supers' tags. An empty query allows none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagQuery AllowedSupers;

	/** Skins that fit the kit, matched against the skin's tags. An empty query allows none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagQuery CompatibleSkins;
};

/** What the rules see of one weapon of the loadout */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutWeaponView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FPrimaryAssetId Id;

	/** The id resolved to an asset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	bool bFound = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FGSTN_AllowedAlternativeView> AllowedAlternatives;
};

/** What the rules see of one super of the loadout */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutSuperView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FPrimaryAssetId Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	bool bFound = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagContainer Tags;
};

/** What the rules see of the skin */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutSkinView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	bool bFound = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGameplayTagContainer Tags;
};

/** The findings of compiling the loadout, as the rules read them */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutTableView
{
	GENERATED_BODY()

	/** Problems of the compile and of the conflict check that make the table unusable (ambiguous bindings and slots among them) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FString> Errors;

	/** Problems that leave the table usable (a slot another always shadows) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FString> Warnings;

	/** Ability keys of the timeline registry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FSoftObjectPath> RegistryAbilities;

	/** Timeline abilities the kit's triggered abilities and passives need in the registry */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FSoftObjectPath> RequiredTimelineAbilities;
};

/**
 * Everything a loadout rule may look at: the loadout and plain values read from the assets it names. Built by the layer that
 * loads assets (GSTNCombatGAS, FGSTN_LoadoutAssembly) outside the simulation, so a rule is a pure function of it and cannot
 * touch the world, an asset or the ability system.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutRulesView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGSTN_LoadoutData Loadout;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGSTN_LoadoutKitView Kit;

	/** In loadout order: index 0 is the kit's primary weapon, 1.. follow Loadout.AdditionalWeapons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FGSTN_LoadoutWeaponView> Weapons;

	/** One per entry of Loadout.Supers, in the same order (an empty slot has an invalid id) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TArray<FGSTN_LoadoutSuperView> Supers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGSTN_LoadoutSkinView Skin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FGSTN_LoadoutTableView Table;
};

/** What a rule found. Errors make the loadout invalid; warnings are shown and allowed */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_LoadoutRuleResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FText> Errors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FText> Warnings;

	bool IsValid() const { return Errors.IsEmpty(); }
};

/**
 * One rule of the loadout validation (combat-m3 8). A rule is a pure function of the view: no world, actor, asset or ability
 * system reads, no latent nodes or timers, no state kept between calls, so the server and every client reach the same verdict
 * for the same loadout. A game adds a rule by deriving a Blueprint class from this one and listing an instance in its rules
 * asset; the built-in rules below are the same kind of object.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, meta = (ShowWorldContextPin = "false"))
class GSTNCOMBATCORE_API UGSTN_LoadoutRule : public UObject
{
	GENERATED_BODY()

public:
	/** The one entry point. The default finds nothing */
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Loadout")
	FGSTN_LoadoutRuleResult Evaluate(const FGSTN_LoadoutRulesView& View) const;
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const;
};

/** The kit exists. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_KitExists : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/** Each additional weapon exists and matches the kit's weapon query, and there are no more than the kit allows. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_AdditionalWeapons : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/** Each chosen super exists and matches the kit's super query, and there is no super beyond the kit's slots. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_Supers : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/** Each customisation swaps a weapon's ability for one of the alternatives that weapon allows for the command. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_Customisations : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/** A chosen skin exists and fits the kit. No skin is fine. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_Skin : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/**
 * The compiled table has no errors: no two bindings or slots the input cannot tell apart (the input plugin's validators,
 * run on the table), no binding the kit's command set cannot take. Shadowed slots are passed on as warnings.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_NoAmbiguousBindings : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/** Every timeline ability the kit can trigger is in the timeline registry. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_LoadoutRule_TimelineRegistry : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override;
};

/**
 * The rules a match validates loadouts with: a list of rule objects, run in order, whose findings are joined. The default
 * list is the built-in rules; a game replaces the asset (or edits its list) to add, drop or reorder rules. Replaceable per game
 * by pointing the loadout component at another rules asset.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_LoadoutRules : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_LoadoutRules();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Rules")
	TArray<TObjectPtr<UGSTN_LoadoutRule>> Rules;

	/** Runs every rule in order, then the registered validators, and joins their errors and warnings. A null entry is skipped */
	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	FGSTN_LoadoutRuleResult Validate(const FGSTN_LoadoutRulesView& View) const;

	/** A check that code outside the rules asset adds (an optional plugin, a game module): pure over the view, like a rule */
	using FValidator = TFunction<FGSTN_LoadoutRuleResult(const FGSTN_LoadoutRulesView&)>;

	/**
	 * Registers a validator that every Validate call runs after the rules, whichever rules asset is in use. Registering an id
	 * again replaces its validator. Validators run in id order (lexical), so the findings are the same on every machine. Call it
	 * from module startup; the registry is read on the game thread only.
	 */
	static void RegisterValidator(FName Id, FValidator Validator);

	/** Removes a validator; returns whether it was registered */
	static bool UnregisterValidator(FName Id);

	/** Number of registered validators */
	static int32 GetValidatorCount();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
