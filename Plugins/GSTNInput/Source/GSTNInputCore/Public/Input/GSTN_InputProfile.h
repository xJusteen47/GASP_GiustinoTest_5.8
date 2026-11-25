// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InputProfile.generated.h"

class UGSTN_ButtonLayout;

/**
 * The kind of device a player plays with. It travels in the player's input profile, so the simulation reads it the same on every machine:
 * the button layout gives each device its own default command for a virtual command (UGSTN_ButtonLayout::CommandChoices), which is how a
 * keyboard player gets an input that a keyboard can actually do (a keyboard "stick" is always fully tilted, so a flick-based command would
 * fire on every press) without any per-frame device bit and without a second code path.
 */
UENUM(BlueprintType)
enum class EGSTN_InputDevice : uint8
{
	Gamepad,
	MouseKeyboard,
};

/** The player's choice of device in the options: follow the last device used, or pin one */
UENUM(BlueprintType)
enum class EGSTN_InputDeviceChoice : uint8
{
	Auto,
	Gamepad,
	MouseKeyboard,
};

/** A layout function moved by the player to another logical button. Only functions the layout lists as remappable are honoured */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_ButtonOverride
{
	GENERATED_BODY()

	/** The layout function ("Movement.Dodge", "Combat.Strong"...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile")
	FName Function;

	/** The logical button the player wants it on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile", meta = (Categories = "Input.Button"))
	FGameplayTag Button;

	bool operator==(const FGSTN_ButtonOverride& Other) const { return Function == Other.Function && Button == Other.Button; }
};

/**
 * The command (the input sequence) the player picked for a virtual command, by the id of one of the alternatives the designer offers for
 * it (UGSTN_ButtonLayout::CommandChoices). The player never writes a sequence: they pick from a closed list, so nothing a player sends
 * can make an input the designer did not allow (owner 2026-11-25: "ovviamente non permettiamo di rimappare ogni cosa").
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_CommandPick
{
	GENERATED_BODY()

	/** The virtual command (a Command.* tag, what the kit's move slots name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile", meta = (Categories = "Command"))
	FGameplayTag VirtualCommand;

	/** Id of the chosen alternative */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile")
	FName Choice;

	bool operator==(const FGSTN_CommandPick& Other) const { return VirtualCommand == Other.VirtualCommand && Choice == Other.Choice; }
};

/**
 * How one player's input is interpreted by the simulation (owner 2026-11-25: "l'handshake all'inizio determina come l'input e i comandi
 * vengono interpretati"). Three independent levels, only the middle one lives here:
 *  1. physical key -> logical button (Input.Button.*): local, UGSTN_KeyBindingsSubsystem; it never travels.
 *  2. logical buttons -> command (the input sequence) of each virtual command: this profile, on top of the match's button layout.
 *  3. virtual command + context -> ability: the kit's data.
 *
 * The profile travels once per change with the player's loadout (a loadout extension in GSTN Combat, then a new loadout revision), so every
 * machine compiles the player's command table from the same profile and adopts it on the same frame. The per-frame input command stays
 * logical buttons and sticks: zero bits per frame for options. Bounded and sanitized against the layout before use, because it comes from
 * a client.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_InputProfile
{
	GENERATED_BODY()

	/** Largest number of overrides or picks a profile may carry (a hostile stream is cut, never allocated) */
	static constexpr int32 MaxEntries = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile")
	EGSTN_InputDevice Device = EGSTN_InputDevice::Gamepad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile", meta = (TitleProperty = "Function"))
	TArray<FGSTN_ButtonOverride> ButtonOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Profile", meta = (TitleProperty = "VirtualCommand"))
	TArray<FGSTN_CommandPick> CommandPicks;

	/**
	 * The profile reduced to what Layout allows: overrides only of remappable functions and onto allowed buttons, picks only of offered
	 * alternatives, one entry per function or virtual command (the first wins), sorted so the same choices always give the same bytes
	 */
	FGSTN_InputProfile Sanitized(const UGSTN_ButtonLayout& Layout) const;

	/** The override of Function, or an empty tag */
	FGameplayTag FindOverride(FName Function) const;

	/** The picked alternative of VirtualCommand, or None */
	FName FindPick(const FGameplayTag& VirtualCommand) const;

	bool operator==(const FGSTN_InputProfile& Other) const
	{
		return Device == Other.Device && ButtonOverrides == Other.ButtonOverrides && CommandPicks == Other.CommandPicks;
	}
	bool operator!=(const FGSTN_InputProfile& Other) const { return !(*this == Other); }
};
