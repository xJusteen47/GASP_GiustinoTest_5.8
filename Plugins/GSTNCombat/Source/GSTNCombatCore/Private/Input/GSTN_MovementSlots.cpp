// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_MovementSlots.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_InputConditions.h"

namespace
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FGSTN_InputSlotSpec MakeSlot(const FGameplayTag& Command, FCondition Anchor)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;
		FGSTN_InputStep Step;
		Step.AllOf.Add(MoveTemp(Anchor));
		Spec.Pattern.Steps.Add(MoveTemp(Step));
		return Spec;
	}

	FCondition MakePress(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(Button);
		return FCondition::Make<FGSTN_InputCondition_Press>(Condition);
	}

	FCondition MakeRelease(const FGameplayTag& Button, int32 MinHeld, int32 MaxHeld)
	{
		FGSTN_InputCondition_Release Condition;
		Condition.Button = Button;
		Condition.MinHeldFrames = MinHeld;
		Condition.MaxHeldFrames = MaxHeld;
		return FCondition::Make<FGSTN_InputCondition_Release>(Condition);
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_MovementSlots::Build(bool bSwapTapHold, int32 HoldFrames)
{
	const int32 Hold = FMath::Max(HoldFrames >= 0 ? HoldFrames : UGSTN_CombatSettings::Get().InteractHoldFrames, 1);
	const FGameplayTag Interact = GSTNCombatButtonTags::Button_Interact.GetTag();
	const FGameplayTag TapCommand = (bSwapTapHold ? GSTNCommandTags::Interact_PickUp : GSTNCommandTags::Interact_Grab).GetTag();
	const FGameplayTag HoldCommand = (bSwapTapHold ? GSTNCommandTags::Interact_Grab : GSTNCommandTags::Interact_PickUp).GetTag();

	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(MakeSlot(GSTNCommandTags::Movement_Dash.GetTag(), MakePress(GSTNCombatButtonTags::Button_Sprint.GetTag())));
	Slots.Add(MakeSlot(GSTNCommandTags::Defence_Dodge.GetTag(), MakePress(GSTNCombatButtonTags::Button_Dodge.GetTag())));
	Slots.Add(MakeSlot(TapCommand, MakeRelease(Interact, 0, Hold - 1)));
	Slots.Add(MakeSlot(HoldCommand, MakeRelease(Interact, Hold, INDEX_NONE)));
	return Slots;
}

#if WITH_EDITOR
void UGSTN_MovementSlots::AppendToSlotSet(UGSTN_InputSlotSet* Set, bool bSwapTapHold)
{
	if (!Set)
	{
		return;
	}

	Set->Modify();
	const TArray<FGSTN_InputSlotSpec> Specs = Build(bSwapTapHold);
	// The same commands as any earlier call: a second call (the swapped set) replaces them instead of duplicating them
	Set->Slots.RemoveAll([](const FGSTN_InputSlotEntry& Entry)
	{
		const FGameplayTag& Command = Entry.GetSpec().Command;
		return Command == GSTNCommandTags::Movement_Dash.GetTag() || Command == GSTNCommandTags::Defence_Dodge.GetTag()
			|| Command == GSTNCommandTags::Interact_Grab.GetTag() || Command == GSTNCommandTags::Interact_PickUp.GetTag();
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
