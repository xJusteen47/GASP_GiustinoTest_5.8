// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_PriorityCriteria.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_InputSettings.generated.h"

/**
 * Project-wide defaults of the input layer (Project Settings > Plugins > GSTN Input).
 *
 * Every value is a designer default: slots and moves override the buffer values individually. All times are fixed-tick
 * frames and every stick threshold is applied to the quantized input (int8 stick, uint8 camera yaw), so the settings
 * must be identical on every machine of a match, like any other config the simulation reads. Frame logic reads them
 * only through this object; it never caches floats across frames.
 *
 * Stored in the plugin's own config file (GSTNInput.ini): the plugin ships its defaults in Config/DefaultGSTNInput.ini and a
 * project overrides them in its Config/DefaultGSTNInput.ini, so nothing about this plugin lives in the project's DefaultGame.ini.
 * Other plugins that need priority tiers or exclusive buttons (combat) own a settings object of their own and contribute
 * to this one at startup, see SetContribution.
 */
UCLASS(config = GSTNInput, defaultconfig, meta = (DisplayName = "GSTN Input"))
class GSTNINPUTCORE_API UGSTN_InputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_InputSettings();

	/** The settings of the running project */
	static const UGSTN_InputSettings& Get() { return *GetDefault<UGSTN_InputSettings>(); }

	/**
	 * Priority tier of a command: the value of the nearest ancestor (or the tag itself) listed in CommandTiers, 0 when none
	 * is. Walks the tag hierarchy upwards and only looks tags up in the maps (the contributions are combined independently of order),
	 * so it gives the same answer on every machine.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|Settings")
	int32 GetCommandTier(const FGameplayTag& Command) const;

	/** Whether the button's presses compete for a single move: listed in ExclusiveButtons or contributed by a plugin */
	UFUNCTION(BlueprintPure, Category = "Input|Settings")
	bool IsExclusiveButton(const FGameplayTag& Button) const;

	/**
	 * Tiers and exclusive buttons that another plugin brings along (combat's, from its own settings), added to the lists below
	 * without touching this plugin's config. Replaces the previous contribution of the same Source. Meant for the settings
	 * object of the running project (call it on GetMutableDefault) once at module startup and when the contributor's settings
	 * change, never during a match, and every machine of a match has to contribute the same values like for any other setting.
	 * @param Source Name of the contributor, e.g. the contributing plugin
	 * @param ExclusiveGroups Exclusive context tag groups of the contributor, added to ExclusiveContextGroups
	 */
	void SetContribution(FName Source, const TMap<FGameplayTag, int32>& Tiers, const TArray<FGameplayTag>& ExclusiveButtons,
		const TArray<FGSTN_ExclusiveTagGroup>& ExclusiveGroups = TArray<FGSTN_ExclusiveTagGroup>());

	/**
	 * ExclusiveContextGroups plus the groups of every contribution. The groups are a set (validation only asks whether two tags
	 * share a group), so the order of the result carries no meaning and the contributions combine independently of order.
	 * Editor-time validation only, it allocates.
	 */
	TArray<FGSTN_ExclusiveTagGroup> GetAllExclusiveContextGroups() const;

	/** Removes what Source contributed */
	void RemoveContribution(FName Source);

	/** The default list: Tier, Specificity, ManualOffset */
	static TArray<TInstancedStruct<FGSTN_PriorityCriterion>> MakeDefaultPriorityCriteria();

	// ---- Buffer

	/** Time buffer: a press stays valid for this many frames (hitstop frames excluded) */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Buffer", meta = (ClampMin = "0"))
	int32 DefaultBufferFrames = 8;

	/**
	 * Hold buffer for the slots that do not override it: a press also stays valid past the time buffer while the button
	 * has been held continuously since it, until the move can come out (as in Smash Ultimate)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Buffer")
	bool bHoldBufferByDefault = true;

	/** Longest a held press can wait in the hold buffer, in frames. 0 = unlimited while held */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Buffer", meta = (ClampMin = "0"))
	int32 MaxHoldBufferFrames = 0;

	/** Tolerance inside a pattern step, e.g. a direction or a modifier held a few frames before or after the press */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Buffer", meta = (ClampMin = "0"))
	int32 DefaultSimultaneityFrames = 3;

	// ---- Priority

	/**
	 * Presses within this many frames of the newest terminal press form one press group: they are "pressed together" and
	 * the command priority picks among them
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority", meta = (ClampMin = "0"))
	int32 SimultaneousFrames = 2;

	/** Which buffered presses compete when several commands match at once */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority")
	EGSTN_BufferConflictPolicy BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;

	/**
	 * How commands that match at once are ranked: the criteria in order, the first one a command scores higher on wins and
	 * the next ones only break ties (equal on all: the slot order). The default, Tier then Specificity then ManualOffset,
	 * gives "a special beats an attack, then the more demanding pattern, then the slot's offset". Reorder or remove entries
	 * to change that; an empty list ranks by slot order alone. At most GSTNPriority::MaxCriteria (8) entries count. Scored once per
	 * slot, so it must be identical on every machine of a match like any other setting the simulation reads
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FGSTN_PriorityCriterion>> PriorityCriteria;

	/**
	 * Tier per command tag root: a higher tier always beats a lower one, whatever the pattern specificity (an attack
	 * and a special pressed together give the special). A command takes the tier of its nearest listed ancestor; a tag
	 * with no listed ancestor is tier 0. Empty by default: a game lists its own here, or a plugin contributes them (the combat
	 * plugin ships its own in its settings)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority", meta = (Categories = "Command"))
	TMap<FGameplayTag, int32> CommandTiers;

	/**
	 * Buttons whose presses compete for a single move: when a command wins, the other unconsumed presses of these buttons
	 * in its press group are consumed with it so the loser does not come out a few frames later. Other buttons (jump,
	 * crouch...) are left alone. Empty by default, like CommandTiers
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority", meta = (Categories = "Input.Button"))
	TArray<FGameplayTag> ExclusiveButtons;

	/**
	 * Legacy: wait a few frames before a single-button command when a chord containing that button could still complete.
	 * Off by default, the startup override window of a move makes chords possible without any input delay
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority")
	bool bChordGraceByDefault = false;

	/** Frames a single button waits for a chord when chord grace is on */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Priority", meta = (ClampMin = "0"))
	int32 ChordGraceFrames = 3;

	// ---- Bindings

	/**
	 * Sets of state tags of which a context holds at most one (Grounded / Airborne). Binding validation uses them to know that
	 * two bindings of a command requiring different members never apply together, so they are not reported as an arbitrary
	 * choice, and to reject a binding requiring two members of one set. Editor-time only, the simulation does not read it. The
	 * plugin ships its own contexts here and the game or another plugin adds the sets of its movement states, so the
	 * validator never names a project tag.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Bindings")
	TArray<FGSTN_ExclusiveTagGroup> ExclusiveContextGroups;

	// ---- Stick

	/** Normalised stick magnitude below which the stick is Neutral */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "1"))
	float StickDeadZone = 0.2f;

	/** Normalised stick magnitude below which the stick is only Aim (a light tilt); from here on it is Tilt */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "1"))
	float AimZone = 0.5f;

	/** A flick is a stick that reaches Tilt at most this many frames after it was last Neutral */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Stick", meta = (ClampMin = "0"))
	int32 FlickFrames = 3;

	/**
	 * Half-width of a diagonal sector around its 45 degree line. 22.5 gives eight equal sectors, less gives wider
	 * cardinal directions (a diagonal needs a more deliberate push)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Stick", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float DiagonalHalfAngle = 22.5f;

	/** World yaw that counts as Up in World space */
	UPROPERTY(Config, EditAnywhere, Category = "Input|Stick", meta = (ForceUnits = "deg"))
	float WorldReferenceYaw = 0.f;

private:
	/** What one plugin contributes; not config, filled at startup, so never saved */
	struct FContribution
	{
		TMap<FGameplayTag, int32> Tiers;
		TArray<FGameplayTag> ExclusiveButtons;
		TArray<FGSTN_ExclusiveTagGroup> ExclusiveGroups;
	};

	/** Every lookup combines all sources the same way (highest tier, any source), so the map's iteration order never shows */
	TMap<FName, FContribution> Contributions;
};
