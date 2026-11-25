// Button layout (IO-2): which logical button (Input.Button.*) triggers which function.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameRules/GSTN_GameRules.h"
#include "Input/GSTN_InputProfile.h"
#include "Input/GSTN_InputSlot.h"

#include "GSTN_ButtonLayout.generated.h"

/** One function of the layout and the logical button that triggers it */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_ButtonAssignment
{
	GENERATED_BODY()

	/**
	 * The function, named by the plugin that reads it ("Held.PickupDrop", "Held.Throw", "Held.KitSwap"...). A name and not a tag so a
	 * plugin adds its functions without declaring tags here
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName Function;

	/** The logical button; the input config maps it to a key and a pad button, and IO-1 lets each player move that key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (Categories = "Input.Button"))
	FGameplayTag Button;
};

/**
 * One command (input sequence) a player may pick for a virtual command. Written like a slot's pattern: with the default buttons of layout
 * functions (LayoutButtons), so the alternative follows the layout and the player's button overrides like any slot does.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_CommandChoice
{
	GENERATED_BODY()

	/** Stable id the player's profile names (never shown; it is what travels) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName Id;

	/** What an options screen shows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FText DisplayName;

	/** The input sequence that replaces the slot's own pattern when this alternative is picked */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FGSTN_InputPattern Pattern;

	/** The buttons of Pattern that follow layout functions, as in FGSTN_InputSlotSpec::LayoutButtons */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Function"))
	TArray<FGSTN_SlotLayoutButton> LayoutButtons;
};

/**
 * The alternatives a designer offers for one virtual command, and which one each device uses when the player picked none. A virtual
 * command without a choice set is not remappable: its command stays the kit slot's own (directional vs neutral, motion of specials: owner
 * 2026-11-25, not everything is remappable).
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_CommandChoiceSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (Categories = "Command"))
	FGameplayTag VirtualCommand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Id"))
	TArray<FGSTN_CommandChoice> Choices;

	/** Alternative used with a gamepad when the player picked none. None = the kit slot's own pattern */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName GamepadDefault;

	/** Alternative used with mouse and keyboard when the player picked none. None = the kit slot's own pattern */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FName MouseKeyboardDefault;

	const FGSTN_CommandChoice* FindChoice(FName Id) const;

	/** The default alternative's id for Device */
	FName GetDefault(EGSTN_InputDevice Device) const { return Device == EGSTN_InputDevice::MouseKeyboard ? MouseKeyboardDefault : GamepadDefault; }

	/** The alternative Profile resolves to: its valid pick, else the device default; null = keep the slot's own pattern */
	const FGSTN_CommandChoice* Resolve(const FGSTN_InputProfile& Profile) const;
};

/**
 * The single button layout: logical button -> function, for every plugin (replaces the button fields that were in Project Settings). A
 * GameRules piece, so a mode or level picks its own layout with its GameRules asset; its class defaults are the shipped layout, from the
 * plugin's DefaultGSTNInput.ini (section [/Script/GSTNInputCore.GSTN_ButtonLayout]; a config class, not in Project Settings). A function
 * missing here falls back to the reading plugin's own default, so defaults never change by omission.
 *
 * Read when the kits compile their slots (every machine, same rules). Per-player choices (IO-3) are meant to layer on top of this piece,
 * per local player, never by editing it.
 */
UCLASS(config = GSTNInput, meta = (DisplayName = "Button layout"))
class GSTNINPUTCORE_API UGSTN_ButtonLayout : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	/** The active layout (the GameRules asset's piece, else these class defaults) */
	static const UGSTN_ButtonLayout& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_ButtonLayout>(); }

	/** The button of Function in the active layout, else Default */
	static FGameplayTag GetButton(FName Function, const FGameplayTag& Default);

	/** The button of Function in this layout, or an empty tag */
	FGameplayTag FindButton(FName Function) const;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (TitleProperty = "Function"))
	TArray<FGSTN_ButtonAssignment> Assignments;

	/**
	 * Functions a player may move to another logical button in their input profile (FGSTN_InputProfile::ButtonOverrides). A function not
	 * listed keeps the layout's button whatever the profile says. Empty = nothing is remappable this way
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Layout|Player")
	TArray<FName> RemappableFunctions;

	/** Virtual commands whose command (input sequence) a player may pick among alternatives, with each device's default */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Layout|Player", meta = (TitleProperty = "VirtualCommand"))
	TArray<FGSTN_CommandChoiceSet> CommandChoices;

	/** The button of Function for a player: their override when the function is remappable, else the layout's */
	FGameplayTag FindButton(FName Function, const FGSTN_InputProfile& Profile) const;

	bool IsRemappable(FName Function) const { return RemappableFunctions.Contains(Function); }

	/** The choice set of VirtualCommand, null when it is not remappable */
	const FGSTN_CommandChoiceSet* FindChoices(const FGameplayTag& VirtualCommand) const;
};
