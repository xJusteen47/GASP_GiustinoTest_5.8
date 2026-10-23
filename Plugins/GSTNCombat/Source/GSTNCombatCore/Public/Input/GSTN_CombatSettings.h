// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_CombatSettings.generated.h"

/**
 * Settings of the combat plugin (Project Settings > Plugins > GSTN Combat). Today they are the combat game's input priority
 * values; they live here, in the combat plugin's own config file, and are contributed to the input plugin's settings, which
 * ships both lists empty. Nothing about the combat plugin lives in the project's DefaultGame.ini.
 *
 * The plugin ships its defaults in Config/DefaultGSTNCombat.ini; a project overrides them in its own
 * Config/DefaultGSTNCombat.ini. The values are pushed into UGSTN_InputSettings (SetContribution) when the module starts and
 * whenever they change in the editor, never during a match, so every machine of a match must run the same values like any
 * other setting the simulation reads.
 */
UCLASS(config = GSTNCombat, defaultconfig, meta = (DisplayName = "GSTN Combat"))
class GSTNCOMBATCORE_API UGSTN_CombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_CombatSettings();

	/** The settings of the running project */
	static const UGSTN_CombatSettings& Get() { return *GetDefault<UGSTN_CombatSettings>(); }

	/** Name under which the combat plugin contributes to the input plugin's settings */
	static FName GetContributionSource();

	/** Hands the lists below to the input plugin's settings, replacing what this plugin contributed before */
	void PushToInputSettings() const;

	/** Takes back what PushToInputSettings contributed (module shutdown) */
	static void RemoveFromInputSettings();

	/**
	 * Priority tier per command tag root, see UGSTN_InputSettings::CommandTiers: a higher tier always beats a lower one
	 * whatever the pattern specificity (a special beats an attack pressed together with it). The default is attack 100, strong
	 * 200, special 300, combo 350, super 400
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Input", meta = (Categories = "Command"))
	TMap<FGameplayTag, int32> CommandTiers;

	/**
	 * Buttons whose presses compete for a single move, see UGSTN_InputSettings::ExclusiveButtons. The default is the attack,
	 * strong and special buttons
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Input", meta = (Categories = "Input.Button"))
	TArray<FGameplayTag> ExclusiveButtons;

	/**
	 * Frames of the startup override window (combat.md 8.3) of a timeline that asks for the default of its command tier
	 * (FGSTN_CombatTimelineData::OverrideFrames = -1), keyed by the tier value of the command tiers above. A tier without an
	 * entry uses the entry of the highest tier below it, and 0 (no window) when there is none. The default is attack 1, strong 2,
	 * special 2
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Timeline", meta = (ClampMin = "0", ForceUnits = "Frames"))
	TMap<int32, int32> OverrideFramesByTier;

	/** The override frames a timeline of a command of this tier gets by default (see OverrideFramesByTier) */
	int32 GetDefaultOverrideFrames(int32 Tier) const;

	/**
	 * The command set a kit starts from when it names no base set of its own (FGSTN_CommandSet::BaseSet). Soft: the plugin ships
	 * it empty, a game points it at its slot set asset, and nothing is loaded until a kit's command set is composed (never
	 * inside the simulation).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Kits")
	TSoftObjectPtr<UGSTN_InputSlotSet> DefaultCommandSet;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
};
