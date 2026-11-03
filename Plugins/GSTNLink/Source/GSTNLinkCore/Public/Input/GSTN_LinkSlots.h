// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputSlot.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_LinkSlots.generated.h"

class UGSTN_InputSlotSet;

/** The requirement a kit's binding of one link command carries (the slot is the input, the requirement the context it means something in) */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkSlotBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link", meta = (Categories = "Command.Link"))
	FGameplayTag Command;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Link")
	FGSTN_TagRequirements Requirements;
};

/**
 * The link commands of the standard command set (m6 spec 8) as slot specs, beside the attack set (UGSTN_StandardSlots) and the movement set
 * (UGSTN_MovementSlots): every row is data, so a kit rebinds them without code.
 *
 *   Command.Link.Grab           Press(Interact)                                  binding requires Interact.Target.Grab
 *   Command.Link.Release        Press(Interact)                                  bindings require Role.Grabber (the grabber lets go) or Role.Latcher,
 *                                                                                each ignoring the other link roles (exclusive for the loadout validator)
 *   Command.Link.Pummel         Press(Attack), stick neutral or lightly pushed   requires Role.Grabber
 *   Command.Link.Throw.Forward  Press(Attack or Strong) + stick tilted forward    requires Role.Grabber
 *   Command.Link.Throw.Back     Press(Attack or Strong) + stick tilted back       requires Role.Grabber
 *   Command.Link.Throw.Up       Press(Strong), stick neutral                     requires Role.Grabber
 *   Command.Link.Throw.Down     Press(Attack) + Crouch held                      requires Role.Grabber (one above the pummel: the crouch shape wins)
 *   Command.Link.Latch.Attack   Press(Attack)                                    requires Role.Latcher
 *
 * Every link slot falls through when unavailable (bFallThroughWhenUnavailable): outside its context it must not reserve the press it shares with
 * a slot below it (the slot resolver otherwise keeps a matched but unavailable slot's press from the lower ones).
 *
 * Grab and Release share the press: the requirements make them exclusive (a grabber has no grab target, the probe projects none while linked),
 * so the same tap grabs and lets go. The role-gated slots carry a priority offset above the attack set's rank, so with the role held the link
 * command wins over Command.Attack.* on the same press; without the role their bindings are not candidates at all. The tap fires on the press
 * (spec 8: the grab must be responsive); the movement set's release-anchored Command.Interact.Grab stays a kit's alternative.
 */
UCLASS()
class GSTNLINKCORE_API UGSTN_LinkSlots : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Added to the role-gated slots' priority so they outrank the standard attack slots of the same press */
	static constexpr int32 RoleSlotPriorityOffset = 100;

	/** The slots listed above, in that order */
	static TArray<FGSTN_InputSlotSpec> Build();

	/** The binding requirement of each row above (Release twice: grabber and latcher), in that order */
	static TArray<FGSTN_LinkSlotBinding> DefaultBindings();

#if WITH_EDITOR
	/** Adds the slots of Build to a set asset (replacing those of the same command), for setup scripts. Null is ignored */
	UFUNCTION(BlueprintCallable, Category = "Link|Slots")
	static void AddToSlotSet(UGSTN_InputSlotSet* Set);
#endif
};
