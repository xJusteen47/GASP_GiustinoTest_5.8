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
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_StandardSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The 12 family commands (per family: Neutral, Directional, CrouchNeutral, CrouchDirectional) followed by the 4 chord commands */
	static TArray<FGSTN_InputSlotSpec> Build();

#if WITH_EDITOR
	/** Replaces the slots of a set asset with the standard command set, to start from or to look at (was an editor button on the set) */
	static void FillSlotSet(UGSTN_InputSlotSet& Set);
#endif
};
