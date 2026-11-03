// Defence, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Attributes/GSTN_AttributeAccessors.h"
#include "GSTN_DefenceAttributeSet.generated.h"

struct FGSTN_ShieldRates;
struct FGSTN_ShieldRules;

/**
 * The shield and grab numbers of a fighter (m6 spec 4). Granted by the loadout only where the mode enables the shield AND the kit
 * has one (UGSTN_DefenceMath::IsShieldAvailable, spec 1d): a PvE fighter or a kit without a shield has no set at all, so nothing
 * of the shield exists on its ability system.
 *
 * Authority split: the per-frame shield value lives in the combat sync state of GSTNDefenceNP, analytically (a stamp and a rate,
 * never a value per frame), so an idle or shielding player costs no per-frame effect. ShieldHealth here is a MIRROR of that value,
 * written by the model through UGSTN_GE_ShieldDamage at sparse events (raise, release, blocked hit, break, every 16 frames while
 * changing) for UI, AI and effects. The model READS the other attributes: Max and the two rates at the end of F-1 (a rate change
 * re-stamps the value, documented approximation of spec 5.3), the damage multiplier when a hit is blocked, the grab resistance
 * when a mash is scaled. An effect that must change the shield value (a pickup) applies UGSTN_GE_ShieldRestore: the model sees the
 * mirror's base value move away from the last value it mirrored and treats the difference as a request (spec 4).
 *
 * Replication is Network Prediction's job in the GAS-NPP fork, so nothing here replicates. Values change only through gameplay
 * effects (and the initial write of the loadout grant, outside the simulation). Clamps follow the fork's idiom: both
 * PreAttributeChange (current value) and PreAttributeBaseChange (base value) apply them, so a reader and a writer never disagree;
 * ShieldHealth stays in [0, MaxShieldHealth] and is pulled down when the maximum drops.
 */
UCLASS()
class GSTNDEFENCEGAS_API UGSTN_DefenceAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGSTN_DefenceAttributeSet();

	/** Mirror of the model's shield value, in [0, MaxShieldHealth]. Written by the model's sparse mirror effect, raised by a restore effect */
	UPROPERTY(BlueprintReadOnly, Category = "Defence|Shield")
	FGameplayAttributeData ShieldHealth;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_DefenceAttributeSet, ShieldHealth)

	/** The shield's full health; FGSTN_ShieldRules::MaxShield at grant. Read by the model as the regeneration cap */
	UPROPERTY(BlueprintReadOnly, Category = "Defence|Shield")
	FGameplayAttributeData MaxShieldHealth;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_DefenceAttributeSet, MaxShieldHealth)

	/** Health regained per frame after the regeneration delay; FGSTN_ShieldRules::RegenPerFrame at grant. Read by the model */
	UPROPERTY(BlueprintReadOnly, Category = "Defence|Shield")
	FGameplayAttributeData ShieldRegenRate;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_DefenceAttributeSet, ShieldRegenRate)

	/** Health lost per frame while the shield is held; FGSTN_ShieldRules::DrainPerFrame at grant. Read by the model */
	UPROPERTY(BlueprintReadOnly, Category = "Defence|Shield")
	FGameplayAttributeData ShieldDrainRate;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_DefenceAttributeSet, ShieldDrainRate)

	/** Scale of the shield damage of every blocked hit (1 = the rules' value). Read by the Shield step through UGSTN_DefenceMath::ShieldDamageOf */
	UPROPERTY(BlueprintReadOnly, Category = "Defence|Shield")
	FGameplayAttributeData IncomingShieldDamageMultiplier;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_DefenceAttributeSet, IncomingShieldDamageMultiplier)

	/** Scales the length of a grab mash (1 = the link rules' value). Read by the link plugin by attribute name, never by type */
	UPROPERTY(BlueprintReadOnly, Category = "Defence|Grab")
	FGameplayAttributeData GrabResistance;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_DefenceAttributeSet, GrabResistance)

	/**
	 * Writes the rules' numbers as the starting values (max and full health, the two rates; the multipliers stay 1), base and
	 * current, bypassing nothing: the clamps hold. For the loadout grant, outside the simulation; never from a tick
	 */
	void InitFromRules(const FGSTN_ShieldRules& Rules);

	/** The three rates and the maximum as the model's math reads them (FGSTN_ShieldRates), from the current values */
	FGSTN_ShieldRates ReadShieldRates(int32 RegenDelayFrames) const;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

private:
	void Clamp(const FGameplayAttribute& Attribute, float& NewValue) const;
};
