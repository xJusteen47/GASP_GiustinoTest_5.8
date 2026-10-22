// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_CombatGASSettings.generated.h"

class UAttributeSet;

/** One starting value of an attribute, addressed by the name of its property in the attribute set. */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_AttributeDefault
{
	GENERATED_BODY()

	/** Property name in the attribute set (for example WalkSpeed). A name the set being initialised does not have is skipped (one list serves both sets). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	FName Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float Value = 0.f;
};

/**
 * Settings of the combat ability layer (Project Settings > Plugins > GSTN Combat GAS). Defaults live in the plugin's
 * Config/DefaultGSTNCombatGAS.ini; a project overrides them in its own copy of that file.
 *
 * A kit's init effect overwrites these starting values with the kit's stats, so they only matter for a pawn that has no kit
 * yet (a character with the attribute sets but no loadout must still be able to move). Every machine of a match must run
 * the same values, like any other setting the simulation reads.
 */
UCLASS(config = GSTNCombatGAS, defaultconfig, meta = (DisplayName = "GSTN Combat GAS"))
class GSTNCOMBATGAS_API UGSTN_CombatGASSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_CombatGASSettings();

	static const UGSTN_CombatGASSettings& Get() { return *GetDefault<UGSTN_CombatGASSettings>(); }

	/**
	 * Starting values of the attribute sets of this plugin, applied when a set is created. Order matters only for meters:
	 * put a meter's maximum before the meter itself so a reader never sees a value above its maximum.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Attributes")
	TArray<FGSTN_AttributeDefault> AttributeDefaults;

	/** Writes AttributeDefaults into the base and current value of Set, bypassing clamps (it runs before the set has an owner). */
	void ApplyAttributeDefaults(UAttributeSet& Set) const;
};
