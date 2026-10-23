// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "Jump/GSTN_JumpTypes.h"
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
	 * Loadout table sets the combat simulation keeps besides the Blueprint's (revision 0): the newest this many. A rollback never
	 * reaches back further than a few frames, so two sets never expire inside the rollback window as long as loadout changes are
	 * spaced by MinFramesBetweenLoadoutChanges. A lookup miss is a closed gate, never a crash
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Loadout", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxRetainedTableRevisions = 4;

	/**
	 * A loadout change within this many simulation frames of the previous one is refused (UGSTN_CombatSimComponent::ServerApplyLoadoutChange):
	 * the table swap of the first must be settled on every machine before the second starts
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Loadout", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MinFramesBetweenLoadoutChanges = 30;

	/**
	 * The command set a kit starts from when it names no base set of its own (FGSTN_CommandSet::BaseSet). Soft: the plugin ships
	 * it empty, a game points it at its slot set asset, and nothing is loaded until a kit's command set is composed (never
	 * inside the simulation).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Kits")
	TSoftObjectPtr<UGSTN_InputSlotSet> DefaultCommandSet;

	/**
	 * The ground speed, in cm/s, that the speeds of the character's movement modes are authored for. The stats movement modifier
	 * scales them by the WalkSpeed stat divided by this, so a combatant whose WalkSpeed equals it moves at the authored speeds.
	 * Equal to the WalkSpeed starting value of the combat attribute defaults unless a game changes one of them.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Stats", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float StatsReferenceWalkSpeed = 600.f;

	/**
	 * Names of floating point properties of the character's movement modes (any mode that has one) that hold a speed the stat scales:
	 * a Blueprint walking mode's gait speeds (walk, run, sprint, crouch) that feed MaxSpeedOverride. Each is assigned its archetype
	 * value times the stat factor on every simulated substep. Empty is valid: the modifier then scales only the plain path every
	 * stock mode reads (UCommonLegacyMovementSettings::MaxSpeed, and MaxSpeedOverride when the mode's archetype sets it). This is
	 * project content: the plugin ships it empty, the project lists the names of its Blueprint modes
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Stats")
	TArray<FName> StatsSpeedProperties;

	/**
	 * How the jump state reacts to a launch by something other than the combatant's own jump (a trampoline). The default refills the air
	 * jumps, clears the coyote window and ends the hold phase of a jump in progress. A kit's own rules replace these once the loadout
	 * carries them; every machine of a match must run the same values
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Jump")
	FGSTN_JumpResetRules JumpResetRules;

	/** Frames after walking off a ledge in which a ground jump is still allowed. 0 = none */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Jump", meta = (ClampMin = "0", ClampMax = "30", ForceUnits = "Frames"))
	int32 JumpCoyoteFrames = 4;

	/** Frames of press that turn an interact tap (grab) into a hold (pick up) in the default movement slots (UGSTN_MovementSlots) */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Interact", meta = (ClampMin = "1", ClampMax = "120", ForceUnits = "Frames"))
	int32 InteractHoldFrames = 12;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
};
