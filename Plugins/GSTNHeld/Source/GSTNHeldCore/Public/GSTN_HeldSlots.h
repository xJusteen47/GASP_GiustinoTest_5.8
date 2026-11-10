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
 * The held commands as slot specs (owner's button design 2026-11-10, subject to playtesting), beside the link set (UGSTN_LinkSlots): every row is
 * data, so a kit rebinds them without code.
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

	/** The slots listed above, in that order */
	static TArray<FGSTN_InputSlotSpec> Build();

	/** The binding requirement of each row above, in that order */
	static TArray<FGSTN_HeldSlotBinding> DefaultBindings();

	/**
	 * The kit bindings of the rows above, each with its slot as an override (a kit's command set does not list the held commands) and the
	 * given abilities in row order (pickup, drop, throw). A command the kit already binds is skipped, so a kit's own choice wins. Appends to
	 * OutBindings and returns how many it added. Used by the plugin's loadout extension on every machine that compiles a kit
	 */
	static int32 AppendKitBindings(const FSoftObjectPath& Pickup, const FSoftObjectPath& Drop, const FSoftObjectPath& Throw,
		TArray<FGSTN_BindingInput>& OutBindings);

	/**
	 * The size classes' command bans (M10 G6): every binding of a banned command (hierarchical match, never a Command.Held.* one) gets the class's
	 * CarryingTag in its ignored tags, restricted to the bindings requiring the ban's OnlyBindingsRequiring when set. A binding that already
	 * requires or ignores the tag is left alone. Returns how many bindings changed
	 */
	static int32 ApplySizeBans(TConstArrayView<FGSTN_HeldSizeClass> SizeClasses, TArray<FGSTN_BindingInput>& InOutBindings);

#if WITH_EDITOR
	/** Adds the slots of Build to a set asset (replacing those of the same command), for setup scripts. Null is ignored */
	UFUNCTION(BlueprintCallable, Category = "Held|Slots")
	static void AddToSlotSet(UGSTN_InputSlotSet* Set);
#endif
};
