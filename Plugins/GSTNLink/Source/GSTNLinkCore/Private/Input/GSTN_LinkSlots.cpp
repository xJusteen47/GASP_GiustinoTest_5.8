// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Input/GSTN_LinkSlots.h"

#include "GSTN_LinkTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"

namespace
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FCondition MakePress(std::initializer_list<FGameplayTag> Buttons)
	{
		FGSTN_InputCondition_Press Condition;
		for (const FGameplayTag& Button : Buttons)
		{
			Condition.Buttons.AddTag(Button);
		}
		return FCondition::Make<FGSTN_InputCondition_Press>(Condition);
	}

	FCondition MakeHold(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Hold Condition;
		Condition.Button = Button;
		Condition.MinFrames = 0;
		return FCondition::Make<FGSTN_InputCondition_Hold>(Condition);
	}

	/** Neutral or lightly pushed (a light push only aims), as the standard set's Neutral shape */
	FCondition MakeNeutralStick()
	{
		FGSTN_InputCondition_Direction Condition;
		Condition.Space = EGSTN_DirectionSpace::Facing;
		Condition.Directions = GSTN_DirectionMasks::All;
		Condition.MinZone = EGSTN_StickZone::Neutral;
		Condition.MaxZone = EGSTN_StickZone::Aim;
		return FCondition::Make<FGSTN_InputCondition_Direction>(Condition);
	}

	FCondition MakeTilted(FGSTN_DirectionMask Directions)
	{
		FGSTN_InputCondition_Direction Condition;
		Condition.Space = EGSTN_DirectionSpace::Facing;
		Condition.Directions = Directions;
		Condition.MinZone = EGSTN_StickZone::Tilt;
		Condition.MaxZone = EGSTN_StickZone::Tilt;
		return FCondition::Make<FGSTN_InputCondition_Direction>(Condition);
	}

	FGSTN_InputSlotSpec MakeSlot(const FGameplayTag& Command, std::initializer_list<FCondition> AllOf, int32 PriorityOffset)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;
		Spec.PriorityOffset = PriorityOffset;
		// A link command means nothing outside its context (no role, no grab target): its press must stay free for the slots below it (the release
		// shares the grab's press, the pummel the attack's), which a matched but unavailable slot would otherwise reserve
		Spec.bFallThroughWhenUnavailable = true;
		FGSTN_InputStep Step;
		for (const FCondition& Condition : AllOf)
		{
			Step.AllOf.Add(Condition);
		}
		Spec.Pattern.Steps.Add(MoveTemp(Step));
		return Spec;
	}

	FGSTN_LinkSlotBinding MakeBinding(const FGameplayTag& Command, const FGameplayTag& Required)
	{
		FGSTN_LinkSlotBinding Binding;
		Binding.Command = Command;
		Binding.Requirements.RequireTags.AddTag(Required);
		return Binding;
	}

	/** A binding of a role, exclusive of the other roles that bind the same command (the loadout validator then sees they never hold together) */
	FGSTN_LinkSlotBinding MakeRoleBinding(const FGameplayTag& Command, const FGameplayTag& Role, std::initializer_list<FGameplayTag> OtherRoles)
	{
		FGSTN_LinkSlotBinding Binding = MakeBinding(Command, Role);
		for (const FGameplayTag& Other : OtherRoles)
		{
			Binding.Requirements.IgnoreTags.AddTag(Other);
		}
		return Binding;
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_LinkSlots::Build()
{
	using namespace GSTNLinkTags;
	const FGameplayTag Interact = GSTNCombatButtonTags::Button_Interact.GetTag();
	const FGameplayTag Attack = GSTNCombatButtonTags::Button_Attack.GetTag();
	const FGameplayTag Strong = GSTNCombatButtonTags::Button_Strong.GetTag();
	const FGameplayTag Crouch = GSTNCombatButtonTags::Button_Crouch.GetTag();
	const int32 Role = RoleSlotPriorityOffset;

	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(MakeSlot(Command_Link_Grab, { MakePress({ Interact }) }, 0));
	Slots.Add(MakeSlot(Command_Link_Release, { MakePress({ Interact }) }, Role));
	Slots.Add(MakeSlot(Command_Link_Pummel, { MakePress({ Attack }), MakeNeutralStick() }, Role));
	Slots.Add(MakeSlot(Command_Link_Throw_Forward, { MakePress({ Attack, Strong }), MakeTilted(GSTN_DirectionMasks::FacingForward) }, Role));
	Slots.Add(MakeSlot(Command_Link_Throw_Back, { MakePress({ Attack, Strong }), MakeTilted(GSTN_DirectionMasks::FacingBack) }, Role));
	Slots.Add(MakeSlot(Command_Link_Throw_Up, { MakePress({ Strong }), MakeNeutralStick() }, Role));
	// Crouch held outranks the plain press (the pummel), as the standard set ranks its crouch shapes above the standing ones
	Slots.Add(MakeSlot(Command_Link_Throw_Down, { MakePress({ Attack }), MakeHold(Crouch) }, Role + 1));
	Slots.Add(MakeSlot(Command_Link_Latch_Attack, { MakePress({ Attack }) }, Role));
	return Slots;
}

TArray<FGSTN_LinkSlotBinding> UGSTN_LinkSlots::DefaultBindings()
{
	using namespace GSTNLinkTags;
	return {
		MakeBinding(Command_Link_Grab, Interact_Target_Grab),
		MakeRoleBinding(Command_Link_Release, Role_Grabber, { Role_Latcher, Role_Latched }),
		MakeRoleBinding(Command_Link_Release, Role_Latcher, { Role_Grabber, Role_Latched }),
		MakeBinding(Command_Link_Pummel, Role_Grabber),
		MakeBinding(Command_Link_Throw_Forward, Role_Grabber),
		MakeBinding(Command_Link_Throw_Back, Role_Grabber),
		MakeBinding(Command_Link_Throw_Up, Role_Grabber),
		MakeBinding(Command_Link_Throw_Down, Role_Grabber),
		MakeBinding(Command_Link_Latch_Attack, Role_Latcher),
	};
}

#if WITH_EDITOR
void UGSTN_LinkSlots::AddToSlotSet(UGSTN_InputSlotSet* Set)
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
