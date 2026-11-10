// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputSlot.h"
#include "Kismet/BlueprintFunctionLibrary.h"

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
	/** Added to the carrying-gated slots' priority so they outrank the attack set on the same press (the link set's offset) */
	static constexpr int32 CarrySlotPriorityOffset = 100;

	/** The slots listed above, in that order */
	static TArray<FGSTN_InputSlotSpec> Build();

	/** The binding requirement of each row above, in that order */
	static TArray<FGSTN_HeldSlotBinding> DefaultBindings();

#if WITH_EDITOR
	/** Adds the slots of Build to a set asset (replacing those of the same command), for setup scripts. Null is ignored */
	UFUNCTION(BlueprintCallable, Category = "Held|Slots")
	static void AddToSlotSet(UGSTN_InputSlotSet* Set);
#endif
};
