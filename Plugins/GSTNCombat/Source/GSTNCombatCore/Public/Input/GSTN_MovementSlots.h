// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_MovementSlots.generated.h"

/**
 * The movement, defence and interaction commands of the default kit as slot specs (combat-m3 section 3). Kept apart from the standard
 * attack set (UGSTN_StandardSlots) because they are kit content: a kit that has no dodge lists Command.Defence.Dodge in
 * CommandSet.RemovedCommands, a kit with a different interact swaps or replaces the slots, and none of it needs code.
 *
 *   Command.Movement.Dash   Press(Input.Button.Sprint)
 *   Command.Defence.Dodge   Press(Input.Button.Dodge); bindings pick the ground or air dodge by context
 *   Interact, tap           Release(Input.Button.Interact) after at most HoldFrames - 1 frames
 *   Interact, hold          Release(Input.Button.Interact) after at least HoldFrames frames
 *
 * The tap and the hold are two anchors on the same release that exclude each other by duration, so a long press can never also
 * fire the tap and a short one is not held back waiting for a hold that cannot come: both resolve on the release. Which command
 * each one yields is data: the default set maps tap to Command.Interact.Grab and hold to Command.Interact.PickUp, and the swapped
 * set exchanges the two commands (bSwapTapHold), no code involved.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_MovementSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The slots listed above. HoldFrames is the press length that turns a tap into a hold; negative = UGSTN_CombatSettings::InteractHoldFrames.
	 * bSwapTapHold gives the tap the pick-up command and the hold the grab command.
	 */
	static TArray<FGSTN_InputSlotSpec> Build(bool bSwapTapHold = false, int32 HoldFrames = -1);

#if WITH_EDITOR
	/**
	 * Adds the slots of Build to a set asset (replacing the ones of the same command), for setup scripts. Exposed to Python like
	 * UGSTN_StandardSlots::FillSlotSet. Null is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Slots")
	static void AppendToSlotSet(UGSTN_InputSlotSet* Set, bool bSwapTapHold = false);
#endif
};
