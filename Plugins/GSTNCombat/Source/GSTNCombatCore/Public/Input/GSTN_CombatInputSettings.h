// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GSTN_CombatCoreTypes.h"
#include "GSTN_CombatInputSettings.generated.h"

/**
 * Project-wide defaults of the combat input layer (Project Settings > Plugins > GSTN Combat Input).
 *
 * Every value is a designer default: slots and moves override the buffer values individually. All times are fixed-tick
 * frames and every stick threshold is applied to the quantized input (int8 stick, uint8 camera yaw), so the settings
 * must be identical on every machine of a match, like any other config the simulation reads. Frame logic reads them
 * only through this object; it never caches floats across frames.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "GSTN Combat Input"))
class GSTNCOMBATCORE_API UGSTN_CombatInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGSTN_CombatInputSettings();

	/** The settings of the running project */
	static const UGSTN_CombatInputSettings& Get() { return *GetDefault<UGSTN_CombatInputSettings>(); }

	/**
	 * Priority tier of a command: the value of the nearest ancestor (or the tag itself) listed in CommandTiers, 0 when none
	 * is. Walks the tag hierarchy upwards, so it never iterates the map and gives the same answer on every machine.
	 */
	int32 GetCommandTier(const FGameplayTag& Command) const;

	// ---- Buffer

	/** Time buffer: a press stays valid for this many frames (hitstop frames excluded) */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Buffer", meta = (ClampMin = "0"))
	int32 DefaultBufferFrames = 8;

	/**
	 * Hold buffer for the slots that do not override it: a press also stays valid past the time buffer while the button
	 * has been held continuously since it, until the move can come out (as in Smash Ultimate)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Buffer")
	bool bHoldBufferByDefault = true;

	/** Longest a held press can wait in the hold buffer, in frames. 0 = unlimited while held */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Buffer", meta = (ClampMin = "0"))
	int32 MaxHoldBufferFrames = 0;

	/** Tolerance inside a pattern step, e.g. a direction or a modifier held a few frames before or after the press */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Buffer", meta = (ClampMin = "0"))
	int32 DefaultSimultaneityFrames = 3;

	// ---- Priority

	/**
	 * Presses within this many frames of the newest terminal press form one press group: they are "pressed together" and
	 * the command priority picks among them
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Priority", meta = (ClampMin = "0"))
	int32 SimultaneousFrames = 2;

	/** Which buffered presses compete when several commands match at once */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Priority")
	EGSTN_BufferConflictPolicy BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;

	/**
	 * Tier per command tag root: a higher tier always beats a lower one, whatever the pattern specificity (an attack
	 * and a special pressed together give the special). A command takes the tier of its nearest listed ancestor; a tag
	 * with no listed ancestor is tier 0
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Priority", meta = (Categories = "Command"))
	TMap<FGameplayTag, int32> CommandTiers;

	/**
	 * Buttons whose presses compete for a single move: when a command wins, the other unconsumed presses of these buttons
	 * in its press group are consumed with it so the loser does not come out a few frames later. Other buttons (jump,
	 * crouch...) are left alone
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Priority", meta = (Categories = "Input.Combat.Button"))
	TArray<FGameplayTag> ExclusiveButtons;

	/**
	 * Legacy: wait a few frames before a single-button command when a chord containing that button could still complete.
	 * Off by default, the startup override window of a move makes chords possible without any input delay
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Priority")
	bool bChordGraceByDefault = false;

	/** Frames a single button waits for a chord when chord grace is on */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Priority", meta = (ClampMin = "0"))
	int32 ChordGraceFrames = 3;

	// ---- Stick

	/** Normalised stick magnitude below which the stick is Neutral */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Stick", meta = (ClampMin = "0", ClampMax = "1"))
	float StickDeadZone = 0.2f;

	/** Normalised stick magnitude below which the stick is only Aim (a light tilt); from here on it is Tilt */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Stick", meta = (ClampMin = "0", ClampMax = "1"))
	float AimZone = 0.5f;

	/** A flick is a stick that reaches Tilt at most this many frames after it was last Neutral */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Stick", meta = (ClampMin = "0"))
	int32 FlickFrames = 3;

	/**
	 * Half-width of a diagonal sector around its 45 degree line. 22.5 gives eight equal sectors, less gives wider
	 * cardinal directions (a diagonal needs a more deliberate push)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Stick", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float DiagonalHalfAngle = 22.5f;

	/** World yaw that counts as Up in World space */
	UPROPERTY(Config, EditAnywhere, Category = "Combat Input|Stick", meta = (ForceUnits = "deg"))
	float WorldReferenceYaw = 0.f;
};
