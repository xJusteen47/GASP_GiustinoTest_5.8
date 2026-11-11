// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_HeldTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Loadout/GSTN_CompiledBindingTable.h"

#include "GSTN_HeldSlots.generated.h"

class UGSTN_InputSlotSet;

/** The requirement a kit's binding of one held command carries */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldSlotBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held", meta = (Categories = "Command.Held"))
	FGameplayTag Command;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held")
	FGSTN_TagRequirements Requirements;
};

/**
 * Which logical buttons (Input.Button.*, mapped to keys and pad buttons by the input config) the held slots read, and the designer slots that
 * replace a built shape (M11 slice 3b, owner rule "no hardcoded inputs"). Filled from UGSTN_HeldSettings (FromSettings); an empty button is the
 * plugin default (Interact for pickup, drop and kit swap, Strong for the throws), so a default-constructed value builds the shipped design
 */
struct GSTNHELDCORE_API FGSTN_HeldSlotInputs
{
	/** Press of this button picks up (object framed) and drops (carrying) */
	FGameplayTag PickupDropButton;
	/** Press of this button throws the object or the lifted enemy (tap = stick, held = camera aim: the ability follows the activating button) */
	FGameplayTag ThrowButton;
	/** Release of this button after the hold swaps for the framed kit */
	FGameplayTag KitSwapButton;
	/** Slots that replace the built one of their command (any Command.Held.* row or Command.Kit.Swap), or null */
	const TArray<FGSTN_InputSlotSpec>* SlotOverrides = nullptr;

	/** The project's choice (Project Settings > Plugins > GSTN Held, Input|Buttons and Kits) */
	static FGSTN_HeldSlotInputs FromSettings();

	FGameplayTag GetPickupDropButton() const;
	FGameplayTag GetThrowButton() const;
	FGameplayTag GetKitSwapButton() const;
	/** The override of Command, or null */
	const FGSTN_InputSlotSpec* FindOverride(const FGameplayTag& Command) const;
};

/**
 * The held commands as slot specs (owner's button design 2026-11-10, subject to playtesting), beside the link set (UGSTN_LinkSlots): every row is
 * data, so a kit rebinds them without code.
 *
 * Default buttons below; FGSTN_HeldSlotInputs (the settings) picks others or replaces a whole slot.
 *
 *   Command.Held.Pickup   Press(Interact)   binding ignores Combat.Held.Carrying; one below the grab, so a grab target wins the press
 *   Command.Held.Drop     Press(Interact)   requires Combat.Held.Carrying (the same button puts the object down)
 *   Command.Held.Throw    Press(Strong)     requires Combat.Held.Carrying, above the strong attacks: tap = left stick, held = camera aim on release
 *
 * Every slot falls through when unavailable, as the link slots do: outside its context it must not keep the press from the slots below it.
 * Not bound by default (hooks for content, plan section 8): the other attack buttons with an object in hand keep the kit's moves; an object
 * definition names what they do with it (UGSTN_HeldObjectDefinition::CommandActions) and its ranged action, read by a later slice.
 */
UCLASS()
class GSTNHELDCORE_API UGSTN_HeldSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The held commands share tier 360 with Command.Link (UGSTN_HeldSettings::CommandTiers), so on a press they share with a link or mount slot
	 * of the same specificity the manual offset decides, and the input validator refuses two such slots at one offset. Taken so far on the
	 * interact press: grab 0, mount -1, release 100, dismount 101, carrier drop 102. Pickup goes below the grab and the mount (a grab or mount
	 * target wins the press); drop and throw above every link role slot (exclusive by requirements anyway: carrying is not a link role)
	 */
	static constexpr int32 PickupSlotPriorityOffset = -2;
	static constexpr int32 CarrySlotPriorityOffset = 110;
	/** The kit swap (M11 K1, tier Command.Kit = 360 in the settings): a release, not a press, below the pickup so no interact offset is shared */
	static constexpr int32 KitSwapSlotPriorityOffset = -3;

	/** The slots listed above, in that order, on the given buttons (default: the plugin's) or replaced by their override */
	static TArray<FGSTN_InputSlotSpec> Build(const FGSTN_HeldSlotInputs& Inputs = FGSTN_HeldSlotInputs());

	/** The binding requirement of each row above, in that order */
	static TArray<FGSTN_HeldSlotBinding> DefaultBindings();

	/**
	 * The kit bindings of the rows above, each with its slot as an override (a kit's command set does not list the held commands) and the
	 * given abilities in row order (pickup, drop, throw). A command the kit already binds is skipped, so a kit's own choice wins. Appends to
	 * OutBindings and returns how many it added. Used by the plugin's loadout extension on every machine that compiles a kit
	 */
	static int32 AppendKitBindings(const FSoftObjectPath& Pickup, const FSoftObjectPath& Drop, const FSoftObjectPath& Throw,
		TArray<FGSTN_BindingInput>& OutBindings, const FGSTN_HeldSlotInputs& Inputs = FGSTN_HeldSlotInputs());

	/**
	 * The kit swap slot (M11 K1): Release(Interact) after at least HoldFrames frames, falling through when unavailable. It shares the release
	 * with the movement set's hold (Command.Interact.PickUp, 12 frames) and wins it by tier while a kit is framed; with no kit framed the
	 * release goes on to the slots below. A release, never a press: the press of the same button stays the grab's and the pickup's
	 */
	static FGSTN_InputSlotSpec BuildKitSwapSlot(int32 HoldFrames, const FGameplayTag& Button = FGameplayTag());

	/** The swap binding's requirements: a framed kit (Interact.Target.Kit), nothing carried (B is the drop then) */
	static FGSTN_TagRequirements KitSwapRequirements();

	/**
	 * Appends the swap binding (slot override, Ability) unless the kit binds Command.Kit.Swap itself or Ability is null. True when added.
	 * Used by the plugin's loadout extension on every machine that compiles a kit, so every kit can swap again after a swap. The slot is the
	 * override of Command.Kit.Swap in Inputs when there is one (HoldFrames is then the override's own), else the release of Inputs' swap button
	 */
	static bool AppendKitSwapBinding(const FSoftObjectPath& Ability, int32 HoldFrames, TArray<FGSTN_BindingInput>& OutBindings,
		const FGSTN_HeldSlotInputs& Inputs = FGSTN_HeldSlotInputs());

	/**
	 * The size classes' command bans (M10 G6): every binding of a banned command (hierarchical match, never a Command.Held.* one) gets the class's
	 * CarryingTag in its ignored tags, restricted to the bindings requiring the ban's OnlyBindingsRequiring when set. A binding that already
	 * requires or ignores the tag is left alone. Returns how many bindings changed
	 */
	static int32 ApplySizeBans(TConstArrayView<FGSTN_HeldSizeClass> SizeClasses, TArray<FGSTN_BindingInput>& InOutBindings);

#if WITH_EDITOR
	/** Adds the slots of Build (on the project's buttons) to a set asset (replacing those of the same command), for setup scripts. Null is ignored */
	UFUNCTION(BlueprintCallable, Category = "Held|Slots")
	static void AddToSlotSet(UGSTN_InputSlotSet* Set);
#endif
};
