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
	Slots.Add(MakeSlot(Command_Held_Pickup, Interact, PickupSlotPriorityOffset));
	Slots.Add(MakeSlot(Command_Held_Drop, Interact, CarrySlotPriorityOffset));
	Slots.Add(MakeSlot(Command_Held_Throw, Strong, CarrySlotPriorityOffset));
	// M10 G9: one above the object throw (never both: an entity grabbing an enemy is not carrying an object), above the grab's kit throws
	Slots.Add(MakeSlot(Command_Held_ThrowEnemy, Strong, CarrySlotPriorityOffset + 1));
	return Slots;
}

TArray<FGSTN_HeldSlotBinding> UGSTN_HeldSlots::DefaultBindings()
{
	using namespace GSTNHeldTags;
	TArray<FGSTN_HeldSlotBinding> Bindings;
	FGSTN_HeldSlotBinding& Pickup = Bindings.AddDefaulted_GetRef();
	Pickup.Command = Command_Held_Pickup;
	Pickup.Requirements.IgnoreTags.AddTag(Held_Carrying);
	// Only with an object in reach (slice 3): otherwise the press falls through to what the kit binds below (the kit swap's hold, the movement set)
	Pickup.Requirements.RequireTags.AddTag(Interact_Target_Pickup);
	FGSTN_HeldSlotBinding& Drop = Bindings.AddDefaulted_GetRef();
	Drop.Command = Command_Held_Drop;
	Drop.Requirements.RequireTags.AddTag(Held_Carrying);
	FGSTN_HeldSlotBinding& Throw = Bindings.AddDefaulted_GetRef();
	Throw.Command = Command_Held_Throw;
	Throw.Requirements.RequireTags.AddTag(Held_Carrying);
	FGSTN_HeldSlotBinding& ThrowEnemy = Bindings.AddDefaulted_GetRef();
	ThrowEnemy.Command = Command_Held_ThrowEnemy;
	ThrowEnemy.Requirements.RequireTags.AddTag(Held_CarryingEnemy);
	ThrowEnemy.Requirements.IgnoreTags.AddTag(Held_Carrying);
	return Bindings;
}

int32 UGSTN_HeldSlots::AppendKitBindings(const FSoftObjectPath& Pickup, const FSoftObjectPath& Drop, const FSoftObjectPath& Throw,
	TArray<FGSTN_BindingInput>& OutBindings)
{
	const TArray<FGSTN_InputSlotSpec> Slots = Build();
	const TArray<FGSTN_HeldSlotBinding> Requirements = DefaultBindings();
	// The enemy throw is the same ability as the object throw (M10 G9): it finds what the holder lifts
	const FSoftObjectPath* Abilities[] = { &Pickup, &Drop, &Throw, &Throw };
	check(Slots.Num() == UE_ARRAY_COUNT(Abilities) && Requirements.Num() == Slots.Num());

	int32 Added = 0;
	for (int32 Row = 0; Row < Slots.Num(); ++Row)
	{
		const FGameplayTag Command = Slots[Row].Command;
		const bool bKitBindsIt = OutBindings.ContainsByPredicate([&Command](const FGSTN_BindingInput& Binding) { return Binding.Command == Command; });
		if (bKitBindsIt || Abilities[Row]->IsNull())
		{
			continue;
		}
		FGSTN_BindingInput& Binding = OutBindings.AddDefaulted_GetRef();
		Binding.Command = Command;
		Binding.Requirements = Requirements[Row].Requirements;
		Binding.Ability = *Abilities[Row];
		Binding.bHasSlotOverride = true;
		Binding.SlotOverride = Slots[Row];
		++Added;
	}
	return Added;
}

FGSTN_InputSlotSpec UGSTN_HeldSlots::BuildKitSwapSlot(int32 HoldFrames)
{
	FGSTN_InputCondition_Release Release;
	Release.Button = GSTNCombatButtonTags::Button_Interact.GetTag();
	Release.MinHeldFrames = FMath::Max(HoldFrames, 1);
	FGSTN_InputSlotSpec Spec;
	Spec.Command = GSTNHeldTags::Command_Kit_Swap;
	Spec.PriorityOffset = KitSwapSlotPriorityOffset;
	Spec.bFallThroughWhenUnavailable = true;
	FGSTN_InputStep Step;
	Step.AllOf.Add(FCondition::Make<FGSTN_InputCondition_Release>(Release));
	Spec.Pattern.Steps.Add(MoveTemp(Step));
	return Spec;
}

FGSTN_TagRequirements UGSTN_HeldSlots::KitSwapRequirements()
{
	FGSTN_TagRequirements Requirements;
	Requirements.RequireTags.AddTag(GSTNHeldTags::Interact_Target_Kit);
	Requirements.IgnoreTags.AddTag(GSTNHeldTags::Held_Carrying);
	Requirements.IgnoreTags.AddTag(GSTNHeldTags::Held_CarryingEnemy);
	return Requirements;
}

bool UGSTN_HeldSlots::AppendKitSwapBinding(const FSoftObjectPath& Ability, int32 HoldFrames, TArray<FGSTN_BindingInput>& OutBindings)
{
	const FGameplayTag Command = GSTNHeldTags::Command_Kit_Swap;
	if (Ability.IsNull() || OutBindings.ContainsByPredicate([&Command](const FGSTN_BindingInput& Binding) { return Binding.Command == Command; }))
	{
		return false;
	}
	FGSTN_BindingInput& Binding = OutBindings.AddDefaulted_GetRef();
	Binding.Command = Command;
	Binding.Requirements = KitSwapRequirements();
	Binding.Ability = Ability;
	Binding.bHasSlotOverride = true;
	Binding.SlotOverride = BuildKitSwapSlot(HoldFrames);
	return true;
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

int32 UGSTN_HeldSlots::ApplySizeBans(TConstArrayView<FGSTN_HeldSizeClass> SizeClasses, TArray<FGSTN_BindingInput>& InOutBindings)
{
	const FGameplayTag HeldCommands = FGameplayTag::RequestGameplayTag(TEXT("Command.Held"), false);
	int32 Changed = 0;
	for (const FGSTN_HeldSizeClass& SizeClass : SizeClasses)
	{
		if (!SizeClass.CarryingTag.IsValid())
		{
			continue;
		}
		for (FGSTN_BindingInput& Binding : InOutBindings)
		{
			if (!Binding.Command.IsValid() || (HeldCommands.IsValid() && Binding.Command.MatchesTag(HeldCommands))
				|| Binding.Requirements.RequireTags.HasTagExact(SizeClass.CarryingTag) || Binding.Requirements.IgnoreTags.HasTagExact(SizeClass.CarryingTag))
			{
				continue;
			}
			for (const FGSTN_HeldCommandBan& Ban : SizeClass.Bans)
			{
				if (Binding.Command.MatchesAny(Ban.Commands)
					&& (!Ban.OnlyBindingsRequiring.IsValid() || Binding.Requirements.RequireTags.HasTag(Ban.OnlyBindingsRequiring)))
				{
					Binding.Requirements.IgnoreTags.AddTag(SizeClass.CarryingTag);
					++Changed;
					break;
				}
			}
		}
	}
	return Changed;
}
