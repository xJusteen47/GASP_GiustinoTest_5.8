// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_StandardSlots.generated.h"

/**
 * The plugin's standard command set (combat.md section 9.1) as slot specs.
 *
 * Three families (normal attack, strong attack, special), each with four input shapes; every slot is one step anchored by
 * the family's button press:
 *   Neutral           stick neutral or lightly pushed (a light push only aims)
 *   Directional       stick tilted in any direction (the move turns towards it)
 *   CrouchNeutral     crouch held, stick neutral or lightly pushed
 *   CrouchDirectional crouch held, stick tilted
 * plus the attack + special chord commands (Command.Combo.AttackSpecial.<Shape>). Directions are judged in Facing space,
 * so "any direction" does not depend on where the character looks. The commands are the same on the ground, in the air or
 * on a wall: bindings pick the move by context. The crouch version outranks the plain one by pattern specificity, the
 * chord by tier.
 *
 * The same builder serves the tests, the validator and FillSlotSet, which creates a set asset from it, so the code and
 * the data cannot drift apart.
 *
 * Buttons (IO-2c): the slots are written with the defaults of the layout functions Combat.Attack, Combat.Strong, Combat.Special and
 * Combat.Crouch and record them in FGSTN_InputSlotSpec::LayoutButtons. The kit compile (FGSTN_BindingTableCompiler) applies the
 * match's button layout to its copies, so the asset never carries the layout of the editor that filled it.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_StandardSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The 12 family commands (per family: Neutral, Directional, CrouchNeutral, CrouchDirectional) followed by the 4 chord commands */
	static TArray<FGSTN_InputSlotSpec> Build();

#if WITH_EDITOR
	/**
	 * Replaces the slots of a set asset with the standard command set, to start from or to look at (was an editor button on
	 * the set). Exposed to Blueprint and Python (editor builds only) so the standard set can be created as an asset by data
	 * setup scripts and editor utilities, with no C++ in the loop. A pointer, not a reference, because that is what a
	 * UFUNCTION takes for an object; null is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Slots")
	static void FillSlotSet(UGSTN_InputSlotSet* Set);

	/**
	 * IO-2c migration: the slots of Set made by this plugin's builders before IO-2c (standard and movement sets: same command, same
	 * pattern as today's builder, no layout buttons) get the layout functions of today's builder, so the asset follows the button
	 * layout. Any other slot, or one a designer changed, is left as it is. Returns how many slots changed; the caller saves the
	 * asset (Tools/GAS/migrate_slot_layout.py). Null is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Slots")
	static int32 AddLayoutFunctionsToSlotSet(UGSTN_InputSlotSet* Set);

	/** AddLayoutFunctionsToSlotSet on a list of entries (a kit's added slots, a set's slots) */
	static int32 AddLayoutFunctions(TArray<FGSTN_InputSlotEntry>& Entries);
#endif
};
