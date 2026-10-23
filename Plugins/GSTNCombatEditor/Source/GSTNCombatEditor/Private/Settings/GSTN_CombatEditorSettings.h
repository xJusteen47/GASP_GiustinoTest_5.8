// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "GSTN_CombatEditorSettings.generated.h"

class UGSTN_PrimaryKitDefinition;
class UGSTN_SkinDefinition;

/** Which axis of root space points in front of the character, for mirroring (combat-m5-editor 8) */
UENUM()
enum class EGSTN_MirrorForwardAxis : uint8
{
	PositiveX,
	NegativeX,
	PositiveY,
	NegativeY
};

/** Colour of one kind of timeline track, addressed by the name of its struct (e.g. GSTN_TimelineTrack_Movement) */
USTRUCT()
struct FGSTN_TrackColor
{
	GENERATED_BODY()

	/** Name of the track struct without the F prefix. A game's own track struct is listed here by its name */
	UPROPERTY(Config, EditAnywhere, Category = "Track")
	FName TrackStruct;

	UPROPERTY(Config, EditAnywhere, Category = "Track")
	FLinearColor Color = FLinearColor::Gray;
};

/** A pair of bone-name fragments that mirror each other (L/R). Matching is by substring, so "_l" pairs "spine_l" with "spine_r". */
USTRUCT()
struct FGSTN_MirrorNamePair
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Mirror")
	FString Left;

	UPROPERTY(Config, EditAnywhere, Category = "Mirror")
	FString Right;

	/** Off for "_l/_r" (rigs disagree on case), on for ".L/.R" where the case is the whole point */
	UPROPERTY(Config, EditAnywhere, Category = "Mirror")
	bool bCaseSensitive = false;
};

/**
 * One line of the Physics Asset to hurtbox zone mapping: a bone whose name contains the fragment gets the zone tag suggested.
 * First match wins, so order the lines from the most to the least specific ("hand" before "arm").
 */
USTRUCT()
struct FGSTN_HurtboxZoneRule
{
	GENERATED_BODY()

	/** Case-insensitive fragment of the bone name, e.g. "hand" */
	UPROPERTY(Config, EditAnywhere, Category = "Hurtbox")
	FString BoneNameContains;

	/** The zone the generator suggests. A game's own tag under the same root works as well: nothing reads a leaf by name */
	UPROPERTY(Config, EditAnywhere, Category = "Hurtbox", meta = (Categories = "Combat.Hurtbox"))
	FGameplayTag Zone;
};

/**
 * Preferences of the combat editor tooling (Project Settings > Plugins > GSTN Combat Editor). Defaults live in the plugin's
 * Config/DefaultGSTNCombatEditor.ini; a project overrides them in its own copy of that file. Editor only: nothing at runtime
 * reads these, and nothing here is part of a simulation, so machines may differ.
 *
 * One class for project-wide and per-user values for now. The per-user ones (auto-key, bake on save, the debugger key) move to
 * a second class with user config when a team needs different values on different desks.
 */
UCLASS(config = GSTNCombatEditor, defaultconfig, meta = (DisplayName = "GSTN Combat Editor"))
class GSTNCOMBATEDITOR_API UGSTN_CombatEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_CombatEditorSettings();

	static const UGSTN_CombatEditorSettings& Get() { return *GetDefault<UGSTN_CombatEditorSettings>(); }

	/** Rate used when the Network Prediction settings cannot be read (the fork is not loaded). The fork's own default */
	static constexpr float DefaultTickRateHz = 62.5f;

	// Preview

	/** Kit shown in the preview when an asset has none of its own. Soft: the editor never loads it until a preview opens */
	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	TSoftObjectPtr<UGSTN_PrimaryKitDefinition> DefaultPreviewKit;

	/** Skin (mesh) shown in the preview when an asset has none of its own */
	UPROPERTY(Config, EditAnywhere, Category = "Preview")
	TSoftObjectPtr<UGSTN_SkinDefinition> DefaultPreviewSkin;

	// Editing

	/** Baking the sockets when a timeline is saved. Off: baking is an explicit button, a stale bake is a validation error */
	UPROPERTY(Config, EditAnywhere, Category = "Editing")
	bool bBakeOnSave = false;

	/** Dragging a hitbox on a frame without a key creates the key first. The numeric fields stay available either way */
	UPROPERTY(Config, EditAnywhere, Category = "Editing")
	bool bAutoKeyDefault = true;

	/** Colours of the track rows by track struct. A struct not listed uses GenericTrackColor */
	UPROPERTY(Config, EditAnywhere, Category = "Editing")
	TArray<FGSTN_TrackColor> TrackColors;

	UPROPERTY(Config, EditAnywhere, Category = "Editing")
	FLinearColor GenericTrackColor = FLinearColor(0.45f, 0.45f, 0.45f);

	// Time

	/**
	 * Frames per second of the simulation tick when the Network Prediction settings cannot be read. The editor never takes the
	 * rate from here when it can read the project's own (GetSimulationTickRate), so a project changes the tick rate in one place.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Time", meta = (ClampMin = "1.0", ForceUnits = "Hz"))
	float FallbackTickRateHz = DefaultTickRateHz;

	// Hurtbox generation and mirroring

	/** Bone-name fragments that mirror each other. Bones that match no pair are reported, never guessed */
	UPROPERTY(Config, EditAnywhere, Category = "Hurtbox")
	TArray<FGSTN_MirrorNamePair> MirrorNamePairs;

	/** The direction the character faces in root space. The mirror plane contains it and the up axis */
	UPROPERTY(Config, EditAnywhere, Category = "Hurtbox")
	EGSTN_MirrorForwardAxis MirrorForwardAxis = EGSTN_MirrorForwardAxis::PositiveX;

	/** Bone-name fragments to hurtbox zone tags, first match wins */
	UPROPERTY(Config, EditAnywhere, Category = "Hurtbox")
	TArray<FGSTN_HurtboxZoneRule> HurtboxZoneRules;

	// Debug

	/** Key that toggles the GSTN combat category in the Gameplay Debugger (E15) */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	FKey DebuggerKey;

	/** The colour of a track struct (name without the F prefix), GenericTrackColor when none is listed */
	FLinearColor GetTrackColor(FName TrackStruct) const;

	/** The zone suggested for a bone name, an invalid tag when no rule matches */
	FGameplayTag SuggestHurtboxZone(FName BoneName) const;

	/**
	 * Frames per second the combat simulation runs at: the Fixed tick rate of the Network Prediction settings (the fork's
	 * UNetworkPredictionSettingsObject, 62.5 Hz in this project), read from the class default object so it needs no world,
	 * and FallbackTickRateHz when the class is not loaded or the value is not usable. Frame f of a timeline starts at
	 * f / rate seconds; the baker (E8) stores the rate it used and the ruler shows milliseconds with it.
	 */
	static float GetSimulationTickRate();

	/**
	 * The reading behind GetSimulationTickRate, apart so a test can feed it any object. SettingsObject is the Network
	 * Prediction settings object (or anything shaped like it: a struct property "Settings" with a numeric "FixedTickFrameRate");
	 * Fallback is returned for null, a missing property or a value that is not a positive finite number.
	 */
	static float ReadFixedTickRate(const UObject* SettingsObject, float Fallback);
};
