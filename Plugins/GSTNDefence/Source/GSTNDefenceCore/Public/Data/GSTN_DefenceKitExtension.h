// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_KitExtension.h"
#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceKitExtension.generated.h"

/**
 * The tech abilities a kit offers, by tech kind. The standard set has no tech (owner 2026-10-23): an empty set means the kit
 * has no tech, no lockout and no tech window, so the shield button during a tumble does nothing.
 *
 * The values are soft classes of the defence GAS module's tech ability. Core has no GameplayAbilities dependency, so the type is
 * the plain UObject class and the GAS layer checks the class when it resolves the entry.
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_TechSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	TMap<EGSTN_TechType, TSoftClassPtr<UObject>> Abilities;

	/** No tech kind has an ability: the kit has no tech */
	bool IsEmpty() const { return Abilities.IsEmpty(); }
};

/**
 * The abilities of getting up from the downed state (m6 spec 6). All three are data of the base ability set or the kit, so
 * changing the rule later is a data change. Core has no GameplayAbilities dependency, so the types are plain UObject classes
 * that the GAS layer checks when it resolves them.
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_GetupSet
{
	GENERATED_BODY()

	/** Get-up roll started by holding the stick in a direction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Getup")
	TSoftClassPtr<UObject> Roll;

	/** Neutral get-up, started by the jump press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Getup")
	TSoftClassPtr<UObject> Neutral;

	/** Get-up attacks, 0..N per moveset. With none the attack button does nothing while downed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Getup")
	TArray<TSoftClassPtr<UObject>> Attacks;
};

/**
 * Gives a fighter its defence, by data (m6 spec 1d). It is an entry of a kit's or weapon's extension array, so the kit asset
 * never grows a property that names defence; a combat build without this plugin keeps the entry and ignores it with a warning.
 *
 * A kit WITHOUT this entry takes the mode's rules and has no shield; a kit with an entry whose ShieldAbility is empty has no
 * shield either, and its own rules asset still applies to the rest (parry profiles, DI, SDI). The references are soft: a
 * dedicated server and a client both resolve them before the pawn's first simulation tick and must resolve the same assets.
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_DefenceKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	/** The kit's own rules (bubble size, drain, perfect-shield window...). Empty = the mode's rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	TSoftObjectPtr<UGSTN_DefenceRules> Rules;

	/** The shield ability class (the defence GAS module's shield ability). Empty = the kit has no shield */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	TSoftClassPtr<UObject> ShieldAbility;

	/** The kit's tech abilities. Empty = no tech */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_TechSet Tech;

	/** The kit's get-up abilities */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_GetupSet GetupSet;

	/** Whether the kit has a shield: the second condition of UGSTN_DefenceMath::IsShieldAvailable */
	bool HasShield() const { return !ShieldAbility.IsNull(); }
};
