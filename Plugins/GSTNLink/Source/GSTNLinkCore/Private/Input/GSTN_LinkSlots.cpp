// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Input/GSTN_LinkSlots.h"
#include "Layout/GSTN_ButtonLayout.h"

#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Loadout/GSTN_CompiledBindingTable.h"

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

	/** The stick entering a facing sector at tilt (a one-element motion: an anchor, so the stick alone can fire a command) */
	FCondition MakeEnter(FGSTN_DirectionMask Directions)
	{
		FGSTN_InputCondition_Motion Condition;
		Condition.Space = EGSTN_DirectionSpace::Facing;
		Condition.Sequence.Add(FGSTN_DirectionSet(Directions));
		Condition.MinZone = EGSTN_StickZone::Tilt;
		return FCondition::Make<FGSTN_InputCondition_Motion>(Condition);
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

namespace
{
	/** IO-2: the button of a link function in the active button layout, else the default */
	FGameplayTag LinkButton(const TCHAR* Function, const FNativeGameplayTag& Default)
	{
		return UGSTN_ButtonLayout::GetButton(Function, Default.GetTag());
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_LinkSlots::Build()
{
	using namespace GSTNLinkTags;
	// IO-2: every button from the button layout (GameRules piece), these defaults when it does not name them
	const FGameplayTag Attack = LinkButton(TEXT("Link.ThrowAttack"), GSTNCombatButtonTags::Button_Attack);
	const FGameplayTag Strong = LinkButton(TEXT("Link.ThrowStrong"), GSTNCombatButtonTags::Button_Strong);
	const FGameplayTag Crouch = LinkButton(TEXT("Link.Crouch"), GSTNCombatButtonTags::Button_Crouch);
	const int32 Role = RoleSlotPriorityOffset;

	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(MakeSlot(Command_Link_Grab, { MakePress({ LinkButton(TEXT("Link.Grab"), GSTNCombatButtonTags::Button_Interact) }) }, 0));
	Slots.Add(MakeSlot(Command_Link_Release, { MakePress({ LinkButton(TEXT("Link.Release"), GSTNCombatButtonTags::Button_Interact) }) }, Role));
	Slots.Add(MakeSlot(Command_Link_Pummel, { MakePress({ LinkButton(TEXT("Link.Pummel"), GSTNCombatButtonTags::Button_Attack) }), MakeNeutralStick() }, Role));
	Slots.Add(MakeSlot(Command_Link_Throw_Forward, { MakePress({ Attack, Strong }), MakeTilted(GSTN_DirectionMasks::FacingForward) }, Role));
	Slots.Add(MakeSlot(Command_Link_Throw_Back, { MakePress({ Attack, Strong }), MakeTilted(GSTN_DirectionMasks::FacingBack) }, Role));
	Slots.Add(MakeSlot(Command_Link_Throw_Up, { MakePress({ Strong }), MakeNeutralStick() }, Role));
	// Crouch held outranks the plain press (the pummel), as the standard set ranks its crouch shapes above the standing ones
	Slots.Add(MakeSlot(Command_Link_Throw_Down, { MakePress({ Attack }), MakeHold(Crouch) }, Role + 1));
	Slots.Add(MakeSlot(Command_Link_Latch_Attack, { MakePress({ LinkButton(TEXT("Link.LatchAttack"), GSTNCombatButtonTags::Button_Attack) }) }, Role));
	return Slots;
}

namespace
{
	struct FThrowBlockers
	{
		FGameplayTagContainer BlockAll;
		FGameplayTagContainer BlockStickOnly;
	};

	TMap<FName, FThrowBlockers>& ThrowBlockers()
	{
		static TMap<FName, FThrowBlockers> Blockers;
		return Blockers;
	}

	/** The settings' slot for this command, else the built one */
	FGSTN_InputSlotSpec SlotOrOverride(const TArray<FGSTN_InputSlotSpec>* Overrides, const FGameplayTag& Command, TFunctionRef<FGSTN_InputSlotSpec()> Build)
	{
		if (Overrides)
		{
			if (const FGSTN_InputSlotSpec* Found = Overrides->FindByPredicate([&Command](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == Command; }))
			{
				return *Found;
			}
		}
		return Build();
	}
}

FGSTN_InputSlotSpec UGSTN_LinkSlots::BuildThrowSlot(const FGameplayTag& ThrowCommand, EGSTN_LinkThrowInputMode Mode)
{
	using namespace GSTNLinkTags;
	const int32 Role = RoleSlotPriorityOffset;
	const bool bForward = ThrowCommand == Command_Link_Throw_Forward;
	const bool bBack = ThrowCommand == Command_Link_Throw_Back;
	const bool bUp = ThrowCommand == Command_Link_Throw_Up;
	const bool bDown = ThrowCommand == Command_Link_Throw_Down;
	const FGameplayTag Special = LinkButton(TEXT("Link.ThrowSpecial"), GSTNCombatButtonTags::Button_Special);
	const FGameplayTag Crouch = LinkButton(TEXT("Link.Crouch"), GSTNCombatButtonTags::Button_Crouch);
	if (Mode == EGSTN_LinkThrowInputMode::Stick)
	{
		if (bForward || bBack)
		{
			return MakeSlot(ThrowCommand, { MakeEnter(bForward ? GSTN_DirectionMasks::FacingForward : GSTN_DirectionMasks::FacingBack) }, Role);
		}
		if (bUp)
		{
			return MakeSlot(ThrowCommand, { MakePress({ LinkButton(TEXT("Link.ThrowJump"), GSTNCombatButtonTags::Button_Jump) }) }, Role);
		}
		if (bDown)
		{
			return MakeSlot(ThrowCommand, { MakePress({ Crouch }) }, Role + 1);
		}
	}
	else if (Mode == EGSTN_LinkThrowInputMode::StickPlusSpecial)
	{
		if (bForward || bBack)
		{
			return MakeSlot(ThrowCommand, { MakePress({ Special }), MakeTilted(bForward ? GSTN_DirectionMasks::FacingForward : GSTN_DirectionMasks::FacingBack) }, Role);
		}
		if (bUp)
		{
			return MakeSlot(ThrowCommand, { MakePress({ Special }), MakeNeutralStick() }, Role);
		}
		if (bDown)
		{
			return MakeSlot(ThrowCommand, { MakePress({ Special }), MakeHold(Crouch) }, Role + 1);
		}
	}
	for (FGSTN_InputSlotSpec& Spec : Build())
	{
		if (Spec.Command == ThrowCommand)
		{
			return MoveTemp(Spec);
		}
	}
	return FGSTN_InputSlotSpec();
}

FGSTN_InputSlotSpec UGSTN_LinkSlots::BuildSpecialCrouchGrabSlot()
{
	return MakeSlot(GSTNLinkTags::Command_Link_Grab_Special,
		{ MakePress({ LinkButton(TEXT("Link.SpecialGrab"), GSTNCombatButtonTags::Button_Special) }),
			MakeHold(LinkButton(TEXT("Link.Crouch"), GSTNCombatButtonTags::Button_Crouch)) }, SpecialGrabPriorityOffset);
}

bool UGSTN_LinkSlots::IsStickOnlyThrow(const FGameplayTag& ThrowCommand, EGSTN_LinkThrowInputMode Mode)
{
	return Mode == EGSTN_LinkThrowInputMode::Stick
		&& (ThrowCommand == GSTNLinkTags::Command_Link_Throw_Forward || ThrowCommand == GSTNLinkTags::Command_Link_Throw_Back);
}

int32 UGSTN_LinkSlots::ApplyKitThrows(const FGSTN_KitThrowFill& Fill, TArray<FGSTN_BindingInput>& Bindings)
{
	using namespace GSTNLinkTags;
	const FGameplayTag Grab = Command_Link_Grab;
	const FGSTN_BindingInput* GrabRow = Bindings.FindByPredicate([&Grab](const FGSTN_BindingInput& Binding)
	{
		return Binding.Command == Grab && !Binding.Ability.IsNull() && !Binding.bHasSlotOverride;
	});
	if (!GrabRow)
	{
		return 0;
	}
	// A copy: the adds below may move the array
	const FGSTN_BindingInput GrabCopy = *GrabRow;

	int32 Bound = 0;
	const FGameplayTag Throws[] = { Command_Link_Throw_Forward, Command_Link_Throw_Back, Command_Link_Throw_Up, Command_Link_Throw_Down };
	for (const FGameplayTag& Command : Throws)
	{
		const FSoftClassPath* KitEntry = Fill.KitThrows ? Fill.KitThrows->Find(Command) : nullptr;
		const bool bKitEntry = KitEntry && KitEntry->IsValid();
		bool bHasRow = false;
		bool bHasBoundRow = false;
		for (FGSTN_BindingInput& Binding : Bindings)
		{
			if (Binding.Command == Command)
			{
				bHasRow = true;
				if (bKitEntry)
				{
					Binding.Ability = *KitEntry;
				}
				bHasBoundRow |= !Binding.Ability.IsNull();
			}
		}
		if (!bHasRow)
		{
			const FSoftClassPath* FallbackEntry = Fill.Fallback ? Fill.Fallback->Find(Command) : nullptr;
			const FSoftClassPath* Entry = bKitEntry ? KitEntry : (FallbackEntry && FallbackEntry->IsValid() ? FallbackEntry : nullptr);
			if (Entry)
			{
				FGSTN_BindingInput& Added = Bindings.AddDefaulted_GetRef();
				Added.Command = Command;
				Added.Requirements.RequireTags.AddTag(Role_Grabber);
				Added.Ability = *Entry;
				bHasBoundRow = true;
			}
		}
		if (!bHasBoundRow)
		{
			continue;
		}
		const bool bStickOnly = IsStickOnlyThrow(Command, Fill.Mode);
		const bool bOverridden = Fill.SlotOverrides && Fill.SlotOverrides->ContainsByPredicate([&Command](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == Command; });
		for (FGSTN_BindingInput& Binding : Bindings)
		{
			if (Binding.Command != Command || Binding.Ability.IsNull())
			{
				continue;
			}
			Binding.Requirements.IgnoreTags.AppendTags(Fill.BlockAll);
			if (bStickOnly)
			{
				Binding.Requirements.IgnoreTags.AppendTags(Fill.BlockStickOnly);
			}
			if (Fill.Mode != EGSTN_LinkThrowInputMode::Buttons || bOverridden)
			{
				Binding.bHasSlotOverride = true;
				Binding.SlotOverride = SlotOrOverride(Fill.SlotOverrides, Command, [&Command, &Fill]() { return BuildThrowSlot(Command, Fill.Mode); });
			}
		}
		++Bound;
	}

	// Its own command (a child of the grab) bound to the grab's ability: a second row of Command.Link.Grab with the same requirement would be
	// ambiguous to the loadout validator, which then refuses the kit's link rows
	const FGameplayTag SpecialGrab = Command_Link_Grab_Special;
	if (Fill.bSpecialCrouchGrab && !Bindings.ContainsByPredicate([&SpecialGrab](const FGSTN_BindingInput& Binding) { return Binding.Command == SpecialGrab; }))
	{
		FGSTN_BindingInput& Special = Bindings.Add_GetRef(GrabCopy);
		Special.Command = SpecialGrab;
		Special.bHasSlotOverride = true;
		Special.SlotOverride = SlotOrOverride(Fill.SlotOverrides, SpecialGrab, []() { return BuildSpecialCrouchGrabSlot(); });
	}
	return Bound;
}

void UGSTN_LinkSlots::SetThrowBlockers(FName Source, const FGameplayTagContainer& BlockAll, const FGameplayTagContainer& BlockStickOnly)
{
	FThrowBlockers& Blockers = ThrowBlockers().FindOrAdd(Source);
	Blockers.BlockAll = BlockAll;
	Blockers.BlockStickOnly = BlockStickOnly;
}

void UGSTN_LinkSlots::ClearThrowBlockers(FName Source)
{
	ThrowBlockers().Remove(Source);
}

void UGSTN_LinkSlots::GetThrowBlockers(FGameplayTagContainer& OutBlockAll, FGameplayTagContainer& OutBlockStickOnly)
{
	for (const TPair<FName, FThrowBlockers>& Pair : ThrowBlockers())
	{
		OutBlockAll.AppendTags(Pair.Value.BlockAll);
		OutBlockStickOnly.AppendTags(Pair.Value.BlockStickOnly);
	}
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
