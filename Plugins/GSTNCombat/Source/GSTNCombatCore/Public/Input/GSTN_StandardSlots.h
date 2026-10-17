// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_StandardSlots.generated.h"

/**
 * The plugin's standard command set (combat.md section 9.1) as slot specs.
 *
 * Three families (normal attack, strong attack, special), grounded and airborne, each with four input shapes; every slot is
 * one step anchored by the family's button press:
 *   Neutral           stick neutral or lightly pushed (a light push only aims)
 *   Directional       stick tilted in any direction (the move turns towards it)
 *   CrouchNeutral     crouch held, stick neutral or lightly pushed
 *   CrouchDirectional crouch held, stick tilted
 * plus the attack + special chord commands (Command.Combo.AttackSpecial.<Shape>, grounded). Directions are judged in Facing
 * space, so "any direction" does not depend on where the character looks. Airborne slots require the Combat.Context.Airborne
 * tag, grounded ones forbid it, so the two sets never compete. The crouch version outranks the plain one by pattern
 * specificity, the chord by tier.
 *
 * The same builder serves the tests, the validator and the editor action that creates a set asset from it, so the code and
 * the data cannot drift apart.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_StandardSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The 24 family commands (per family: grounded then airborne) followed by the 4 chord commands */
	static TArray<FGSTN_InputSlotSpec> Build();
};
