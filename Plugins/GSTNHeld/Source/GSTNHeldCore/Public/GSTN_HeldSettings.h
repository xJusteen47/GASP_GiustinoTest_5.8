// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GSTN_HeldTypes.h"

#include "GSTN_HeldSettings.generated.h"

/**
 * Project defaults of held objects (Project Settings > Plugins > GSTN Held, saved in the plugin's DefaultGSTNHeld.ini). Everything here is a TEST
 * VALUE of the owner's 2026-11-10 button design, kept as data so playtesting changes it without code.
 */
UCLASS(config = GSTNHeld, defaultconfig, meta = (DisplayName = "GSTN Held"))
class GSTNHELDCORE_API UGSTN_HeldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UGSTN_HeldSettings& Get() { return *GetDefault<UGSTN_HeldSettings>(); }

	/** When a holder drops what it holds without wanting to; an object's own tuning can override it (bOverrideBreakRules) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Break")
	FGSTN_HeldBreakRules DefaultBreakRules;

	/** How the strong button throws (tap = stick, held = camera aim with the reticle) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Throw")
	FGSTN_HeldThrowInput ThrowInput;

	/**
	 * The kits that get the held bindings (pickup, drop, throw on the default buttons, UGSTN_HeldSlots) through the plugin's loadout extension
	 * when they are compiled, so no kit Blueprint is edited by hand. A kit that binds a held command itself keeps its own binding for that
	 * command. A list, not every kit: a kit that never sees an object keeps its table exactly as authored
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowedClasses = "/Script/GSTNCombatGAS.GSTN_PrimaryKitDefinition"))
	TArray<FSoftObjectPath> KitsWithDefaultBindings;

	/** Every compiled kit gets them, listed or not */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bAddDefaultBindingsToEveryKit = false;

	/** Whether the kit asset at Kit gets the held bindings (the list above, or every kit) */
	bool WantsDefaultBindings(const FSoftObjectPath& Kit) const
	{
		return bAddDefaultBindingsToEveryKit || (!Kit.IsNull() && KitsWithDefaultBindings.Contains(Kit));
	}

	/** The abilities those bindings start; empty = the plugin's own (UGSTN_GA_HeldPickup, UGSTN_GA_HeldDrop, UGSTN_GA_HeldThrow) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath PickupAbility;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath DropAbility;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (MetaClass = "/Script/GameplayAbilities.GameplayAbility"))
	FSoftClassPath ThrowAbility;

	/**
	 * Priority tier of the held commands, handed to the input settings at startup (as GSTNLink does for Command.Link). Without it the held
	 * commands have tier 0 and every strong attack (tier 200) takes the strong press from the throw
	 */
	UPROPERTY(config, EditAnywhere, Category = "Input", meta = (Categories = "Command"))
	TMap<FGameplayTag, int32> CommandTiers;

	/** Hands CommandTiers to the input settings under this plugin's contribution source */
	void PushToInputSettings() const;
	static void RemoveFromInputSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
