// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldSlots.h"

#include "GSTN_HeldTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"

namespace
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FCondition MakePress(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(Button);
		return FCondition::Make<FGSTN_InputCondition_Press>(Condition);
	}

	FGSTN_InputSlotSpec MakeSlot(const FGameplayTag& Command, const FGameplayTag& Button, int32 PriorityOffset)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;
		Spec.PriorityOffset = PriorityOffset;
		// A held command means nothing outside its context: its press stays free for the slots below it (the grab, the strong attacks)
		Spec.bFallThroughWhenUnavailable = true;
		FGSTN_InputStep Step;
		Step.AllOf.Add(MakePress(Button));
		Spec.Pattern.Steps.Add(MoveTemp(Step));
		return Spec;
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_HeldSlots::Build()
{
	using namespace GSTNHeldTags;
	const FGameplayTag Interact = GSTNCombatButtonTags::Button_Interact.GetTag();
	const FGameplayTag Strong = GSTNCombatButtonTags::Button_Strong.GetTag();
	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(MakeSlot(Command_Held_Pickup, Interact, -1));
	Slots.Add(MakeSlot(Command_Held_Drop, Interact, CarrySlotPriorityOffset));
	Slots.Add(MakeSlot(Command_Held_Throw, Strong, CarrySlotPriorityOffset));
	return Slots;
}

TArray<FGSTN_HeldSlotBinding> UGSTN_HeldSlots::DefaultBindings()
{
	using namespace GSTNHeldTags;
	TArray<FGSTN_HeldSlotBinding> Bindings;
	FGSTN_HeldSlotBinding& Pickup = Bindings.AddDefaulted_GetRef();
	Pickup.Command = Command_Held_Pickup;
	Pickup.Requirements.IgnoreTags.AddTag(Held_Carrying);
	FGSTN_HeldSlotBinding& Drop = Bindings.AddDefaulted_GetRef();
	Drop.Command = Command_Held_Drop;
	Drop.Requirements.RequireTags.AddTag(Held_Carrying);
	FGSTN_HeldSlotBinding& Throw = Bindings.AddDefaulted_GetRef();
	Throw.Command = Command_Held_Throw;
	Throw.Requirements.RequireTags.AddTag(Held_Carrying);
	return Bindings;
}

#if WITH_EDITOR
void UGSTN_HeldSlots::AddToSlotSet(UGSTN_InputSlotSet* Set)
{
	if (!Set)
	{
		return;
	}
	Set->Modify();
	const TArray<FGSTN_InputSlotSpec> Specs = Build();
	Set->Slots.RemoveAll([&Specs](const FGSTN_InputSlotEntry& Entry)
	{
		return Specs.ContainsByPredicate([&Entry](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == Entry.GetSpec().Command; });
	});
	for (const FGSTN_InputSlotSpec& Spec : Specs)
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline = Spec;
		Set->Slots.Add(MoveTemp(Entry));
	}
	Set->MarkPackageDirty();
}
#endif
