// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_LinkSettings.generated.h"

/**
 * How a grabber picks one of its four throws (M11 K2, owner decision 2 of 2026-11-11). Every machine of a match must use the same mode: it
 * shapes the slots of the compiled binding table, which the prediction replays.
 */
UENUM(BlueprintType)
enum class EGSTN_LinkThrowInputMode : uint8
{
	/**
	 * Smash style, the default: in a grab the stick alone picks the throw. Stick pushed forward = forward throw, pulled back = back throw
	 * (the moment it enters the sector, so a stick already held when the grab lands does not throw at once); jump = up throw, crouch = down
	 * throw (a 3D stick has no up and down of its own). Attack is still the pummel
	 */
	Stick,
	/** The stick plus the special button: special + forward / back, special alone = up, special + crouch held = down */
	StickPlusSpecial,
	/** The F12 shapes of the link slot set as authored (attack or strong + tilt, strong = up, crouch + attack = down): no slot is overridden */
	Buttons,
	/**
	 * grab-link (owner 2026-11-23): the project's move notation, with the stick left free to WALK while holding. Attack neutral = N. throw, attack +
	 * stick tilted = F. throw, attack + crouch held = Cr. throw, attack + crouch held + stick tilted = Cr.F. throw. No pummel (owner 2026-11-23: removed).
	 * Which way F. goes is the grab turn ergonomics (EGSTN_GrabTurnErgonomics). Up and back throws are not bound (back only with BackThrow)
	 */
	Notation
};

/**
 * How a grabber turns before a throw in Notation mode (grab-link, owner 2026-11-23: "per ora ergonomia istantanea"). Both reuse the move start aim
 * (EGSTN_AimPolicy on the throw's timeline: SnapIfAimOrTilt, MaxTurnDegrees 180), so there is no second turning logic: the mode only decides which
 * stick directions fire the F. throw slot.
 */
UENUM(BlueprintType)
enum class EGSTN_GrabTurnErgonomics : uint8
{
	/** F. throw in EVERY direction: attack + any tilt fires F., whose start snaps the grabber to the stick. No back throw (default) */
	Instant,
	/** F. only on a forward tilt; a back tilt fires the B. throw (Command.Link.Throw.Back) */
	BackThrow
};

/**
 * Settings of the link plugin (Project Settings > Plugins > GSTN Link). Defaults ship in the plugin's Config/DefaultGSTNLink.ini;
 * a project overrides them in its own Config/DefaultGSTNLink.ini.
 *
 * The simulation of every machine reads these values, so they must be the same on all machines of a match and must never change
 * during one, like any other setting the simulation reads. Link types are not listed here: the asset manager finds them by their
 * primary asset type (GSTNLinkAssetTypes::LinkType), which the module registers itself (see LinkTypeDirectories).
 */
UCLASS(config = GSTNLink, defaultconfig, meta = (DisplayName = "GSTN Link"))
class GSTNLINKCORE_API UGSTN_LinkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The settings of the running project */
	static const UGSTN_LinkSettings& Get() { return *GetDefault<UGSTN_LinkSettings>(); }

	/** Frames a predicted link waits for its response before the link tick clears it */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Formation", meta = (ClampMin = "1", ClampMax = "120", ForceUnits = "Frames"))
	int32 RequestTimeoutFrames = 12;

	/** Frames after a formation during which a one-sided link is not yet an orphan (the other side's state is still in flight) */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Watchdog", meta = (ClampMin = "0", ClampMax = "60", ForceUnits = "Frames"))
	int32 OrphanGraceFrames = 3;

	/**
	 * How far a placement the player's machine predicts from its view (an initiator-placed exit, a per-link anchor: M7 G6) may be from the
	 * authority's, placed from the lag-compensated view, before it counts as a misprediction. Both come from interpolated poses, so they never
	 * agree bit for bit; inside this they are the same placement and nothing is corrected
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Prediction", meta = (ClampMin = "0.0", ClampMax = "50.0", ForceUnits = "cm"))
	float PredictedPlacementToleranceCm = 5.f;

	/**
	 * Content folders, besides the plugin's own content, where link type definitions live (a project's test or game content, another
	 * plugin's content). The module registers the GSTN_LinkType primary asset type with the asset manager over the plugin's content
	 * and these folders at startup, so the project's DefaultGame.ini does not list the type: a PrimaryAssetTypesToScan line naming a
	 * class of this plugin trips an engine ensure in every build where the plugin is disabled. Read once, when the asset manager is
	 * created; a change needs an editor restart
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Assets", meta = (ContentDir, LongPackageName))
	TArray<FDirectoryPath> LinkTypeDirectories;

	/**
	 * Priority tier of the link command roots, contributed to the GSTN Input settings at startup (as the combat plugin contributes its own): a
	 * higher tier always beats a lower one, so a link command (Command.Link: the pummel on the attack press, the throws on attack and strong)
	 * outranks the attack it shares a press with. Every link slot falls through when it cannot execute, so outside its role it costs nothing
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Input", meta = (Categories = "Command"))
	TMap<FGameplayTag, int32> CommandTiers;

	/**
	 * Kit throws (M11 K2): how the grabber picks a throw. Applied when a loadout is compiled, to every kit asset that binds the grab
	 * (Command.Link.Grab), so no kit is edited by hand. Read at compile time: a change takes effect at the next kit swap or PIE
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	EGSTN_LinkThrowInputMode ThrowInputMode = EGSTN_LinkThrowInputMode::Stick;

	/** Off = kits keep exactly the throw rows they author (no fallback rows, no kit set, no slot override, no special grab) */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	bool bApplyKitThrows = true;

	/**
	 * The common throw set (owner decision 3): a grabbing kit that binds no ability to a throw, and whose link extension names none, gets
	 * this one. The plugin's defaults are the placeholder C++ throws of GSTNLinkNP (UGSTN_GA_LinkThrowForward...)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	FGSTN_LinkThrowSet FallbackThrows;

	/**
	 * Owner decision 2: besides the tap of the grab button, special + crouch held grabs too, while a grab target is framed (it then wins over
	 * the down special). Off = the grab button only
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	bool bGrabWithSpecialCrouch = true;

	/**
	 * Designer slots that replace the mode's built shape for their command: any of Command.Link.Throw.* and Command.Link.Grab.Special (the
	 * second grab input; the tap of the grab button stays the slot set's). Empty = the mode's shapes (UGSTN_LinkSlots::BuildThrowSlot). The
	 * buttons in a slot are logical (Combat.Button.*): the input config maps them to keys and pad buttons
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	TArray<FGSTN_InputSlotSpec> ThrowSlotOverrides;

	/** grab-link: Notation mode only. Overridden by the cvar gstn.Grab.TurnMode (GSTN Debug) at the next kit compile */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	EGSTN_GrabTurnErgonomics GrabTurnErgonomics = EGSTN_GrabTurnErgonomics::Instant;

	/**
	 * grab-link (owner 2026-11-23, "con il tasto della schivata, credo": TO CONFIRM): the dodge press, while grabbing, sets the partner down without a
	 * throw. It binds Command.Link.Release.PutDown to the kit's own Release ability (the release reason and the ring exit, as the interact tap)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Throws")
	bool bPutDownWithDodge = true;

	/** grab-link: how every kit holds its grabbed partner (by the neck by default); a kit's link extension can replace it (bOverrideHoldPose) */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Hold")
	FGSTN_GrabHoldPose HoldPose;

	/**
	 * The grabbed entity's bone or socket placed on the hold point (the reference pose's, baked at attach with the role sockets). A mesh without it
	 * holds by the root (the type's offset then decides alone)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Hold")
	FName NeckSocketName = TEXT("neck_01");

	/** grab-link: the grabber's walk speed multiplier while holding (UGSTN_GE_GrabHold). TEST VALUE; cvar gstn.Grab.WalkScale */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Hold", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float HoldWalkScale = 0.75f;

	/** grab-link: the grabber's jump speed multiplier while holding (owner: "salto più basso"). TEST VALUE; cvar gstn.Grab.JumpScale */
	UPROPERTY(Config, EditAnywhere, Category = "Link|Hold", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float HoldJumpScale = 0.7f;

	/**
	 * grab-link playtest overrides from the GSTN Debug window (gstn.Grab.*), one control per option. They are read by the simulation at the
	 * points named on each, so all machines must have the same values: true in a PIE session (one process); a networked test sets them on every
	 * machine. 0 (a negative scale) means "the data's own"
	 */
	EGSTN_GrabTurnErgonomics GetEffectiveGrabTurn() const;
	float GetEffectiveHoldWalkScale() const;
	float GetEffectiveHoldJumpScale() const;
	/** The hold pose a grabber with this kit extension (may be null) uses, with the cvar gstn.Grab.HoldByNeck applied */
	FGSTN_GrabHoldPose GetEffectiveHoldPose(const FGSTN_LinkKitExtension* GrabberKit) const;
	/** gstn.Grab.MaxHoldFrames: when above 0, replaces the mash rule's MaxFrames AND caps its initial frames (the hold ends by then without mashing) */
	static int32 GetMaxHoldFramesOverride();
	/** gstn.Grab.EscapeHitForAll: every grabbed entity hits its grabber on escape, with its kit's hit data (or the default), for testing on mannequins */
	static bool GetEscapeHitForAll();

	/** Hands CommandTiers to the input settings under this plugin's contribution source */
	void PushToInputSettings() const;
	static void RemoveFromInputSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
