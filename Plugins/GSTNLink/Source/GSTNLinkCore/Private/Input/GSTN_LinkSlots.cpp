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
	/** A layout function of a link slot and its default button */
	using FLayoutFunction = TPair<const TCHAR*, const FNativeGameplayTag*>;

	/**
	 * IO-2c: the slot keeps its default buttons and records which layout function each one is; the kit compile resolves them with the
	 * match's button layout (FGSTN_InputSlotSpec::ApplyButtonLayout), so slots saved on a kit asset follow the GameRules too
	 */
	FGSTN_InputSlotSpec WithLayout(FGSTN_InputSlotSpec Spec, std::initializer_list<FLayoutFunction> Functions)
	{
		for (const FLayoutFunction& Function : Functions)
		{
			Spec.AddLayoutButton(Function.Key, Function.Value->GetTag());
		}
		return Spec;
	}
}

TArray<FGSTN_InputSlotSpec> UGSTN_LinkSlots::Build()
{
	using namespace GSTNLinkTags;
	// IO-2: the defaults of the layout functions; the kit compile swaps in the match's layout (IO-2c, WithLayout)
	const FNativeGameplayTag& Interact = GSTNCombatButtonTags::Button_Interact;
	const FNativeGameplayTag& Attack = GSTNCombatButtonTags::Button_Attack;
	const FNativeGameplayTag& Strong = GSTNCombatButtonTags::Button_Strong;
	const FNativeGameplayTag& Crouch = GSTNCombatButtonTags::Button_Crouch;
	const FLayoutFunction ThrowAttack(TEXT("Link.ThrowAttack"), &Attack);
	const FLayoutFunction ThrowStrong(TEXT("Link.ThrowStrong"), &Strong);
	const int32 Role = RoleSlotPriorityOffset;

	TArray<FGSTN_InputSlotSpec> Slots;
	Slots.Add(WithLayout(MakeSlot(Command_Link_Grab, { MakePress({ Interact }) }, 0), { { TEXT("Link.Grab"), &Interact } }));
	Slots.Add(WithLayout(MakeSlot(Command_Link_Release, { MakePress({ Interact }) }, Role), { { TEXT("Link.Release"), &Interact } }));
	Slots.Add(WithLayout(MakeSlot(Command_Link_Pummel, { MakePress({ Attack }), MakeNeutralStick() }, Role), { { TEXT("Link.Pummel"), &Attack } }));
	Slots.Add(WithLayout(MakeSlot(Command_Link_Throw_Forward, { MakePress({ Attack, Strong }), MakeTilted(GSTN_DirectionMasks::FacingForward) }, Role), { ThrowAttack, ThrowStrong }));
	Slots.Add(WithLayout(MakeSlot(Command_Link_Throw_Back, { MakePress({ Attack, Strong }), MakeTilted(GSTN_DirectionMasks::FacingBack) }, Role), { ThrowAttack, ThrowStrong }));
	Slots.Add(WithLayout(MakeSlot(Command_Link_Throw_Up, { MakePress({ Strong }), MakeNeutralStick() }, Role), { ThrowStrong }));
	// Crouch held outranks the plain press (the pummel), as the standard set ranks its crouch shapes above the standing ones
	Slots.Add(WithLayout(MakeSlot(Command_Link_Throw_Down, { MakePress({ Attack }), MakeHold(Crouch) }, Role + 1), { ThrowAttack, { TEXT("Link.Crouch"), &Crouch } }));
	Slots.Add(WithLayout(MakeSlot(Command_Link_Latch_Attack, { MakePress({ Attack }) }, Role), { { TEXT("Link.LatchAttack"), &Attack } }));
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

FGSTN_KitThrowFill::FGSTN_KitThrowFill(EGSTN_LinkThrowInputMode InMode)
	: Mode(InMode)
	, Ergonomics(EGSTN_GrabTurnErgonomics::Instant)
{
}

FGSTN_InputSlotSpec UGSTN_LinkSlots::BuildThrowSlot(const FGameplayTag& ThrowCommand, EGSTN_LinkThrowInputMode Mode, EGSTN_GrabTurnErgonomics Ergonomics)
{
	using namespace GSTNLinkTags;
	if (Mode != EGSTN_LinkThrowInputMode::Notation)
	{
		return BuildThrowSlot(ThrowCommand, Mode);
	}
	const int32 Role = RoleSlotPriorityOffset;
	const FNativeGameplayTag& Attack = GSTNCombatButtonTags::Button_Attack;
	const FNativeGameplayTag& Crouch = GSTNCombatButtonTags::Button_Crouch;
	const FLayoutFunction ThrowAttack(TEXT("Link.ThrowAttack"), &Attack);
	const FLayoutFunction LinkCrouch(TEXT("Link.Crouch"), &Crouch);
	const bool bInstant = Ergonomics == EGSTN_GrabTurnErgonomics::Instant;
	// Instant: F. answers every tilt and its start aim (SnapIfAimOrTilt) turns the grabber to the stick; with a back throw the back sector is B.'s
	const FGSTN_DirectionMask ForwardMask = bInstant ? GSTN_DirectionMasks::AnyTilted
		: static_cast<FGSTN_DirectionMask>(GSTN_DirectionMasks::AnyTilted & ~GSTN_DirectionMasks::FacingBack);
	if (ThrowCommand == Command_Link_Throw_Neutral)
	{
		// One above the slot set's pummel shape (attack + neutral), whose rows Notation hides: the N. throw never depends on the fall-through
		return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Attack }), MakeNeutralStick() }, Role + 1), { ThrowAttack });
	}
	if (ThrowCommand == Command_Link_Throw_Forward)
	{
		return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Attack }), MakeTilted(ForwardMask) }, Role + 1), { ThrowAttack });
	}
	if (ThrowCommand == Command_Link_Throw_Back && !bInstant)
	{
		return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Attack }), MakeTilted(GSTN_DirectionMasks::FacingBack) }, Role + 1), { ThrowAttack });
	}
	if (ThrowCommand == Command_Link_Throw_Down)
	{
		return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Attack }), MakeHold(Crouch), MakeNeutralStick() }, Role + 2), { ThrowAttack, LinkCrouch });
	}
	if (ThrowCommand == Command_Link_Throw_CrouchForward)
	{
		return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Attack }), MakeHold(Crouch), MakeTilted(GSTN_DirectionMasks::AnyTilted) }, Role + 3),
			{ ThrowAttack, LinkCrouch });
	}
	return FGSTN_InputSlotSpec();
}

TArray<FGameplayTag> UGSTN_LinkSlots::GetThrowCommands(EGSTN_LinkThrowInputMode Mode, EGSTN_GrabTurnErgonomics Ergonomics)
{
	using namespace GSTNLinkTags;
	if (Mode == EGSTN_LinkThrowInputMode::Notation)
	{
		TArray<FGameplayTag> Commands = { Command_Link_Throw_Neutral, Command_Link_Throw_Forward, Command_Link_Throw_Down, Command_Link_Throw_CrouchForward };
		if (Ergonomics == EGSTN_GrabTurnErgonomics::BackThrow)
		{
			Commands.Add(Command_Link_Throw_Back);
		}
		return Commands;
	}
	return { Command_Link_Throw_Forward, Command_Link_Throw_Back, Command_Link_Throw_Up, Command_Link_Throw_Down };
}

FGSTN_InputSlotSpec UGSTN_LinkSlots::BuildPutDownSlot()
{
	// One above the role slots: the dodge's own command (its tier is below Command.Link anyway) never reads the press while holding
	return WithLayout(MakeSlot(GSTNLinkTags::Command_Link_Release_PutDown, { MakePress({ GSTNCombatButtonTags::Button_Dodge }) }, RoleSlotPriorityOffset),
		{ { TEXT("Link.PutDown"), &GSTNCombatButtonTags::Button_Dodge } });
}

FGSTN_InputSlotSpec UGSTN_LinkSlots::BuildThrowSlot(const FGameplayTag& ThrowCommand, EGSTN_LinkThrowInputMode Mode)
{
	using namespace GSTNLinkTags;
	const int32 Role = RoleSlotPriorityOffset;
	const bool bForward = ThrowCommand == Command_Link_Throw_Forward;
	const bool bBack = ThrowCommand == Command_Link_Throw_Back;
	const bool bUp = ThrowCommand == Command_Link_Throw_Up;
	const bool bDown = ThrowCommand == Command_Link_Throw_Down;
	const FNativeGameplayTag& Special = GSTNCombatButtonTags::Button_Special;
	const FNativeGameplayTag& Crouch = GSTNCombatButtonTags::Button_Crouch;
	const FLayoutFunction ThrowSpecial(TEXT("Link.ThrowSpecial"), &Special);
	const FLayoutFunction LinkCrouch(TEXT("Link.Crouch"), &Crouch);
	if (Mode == EGSTN_LinkThrowInputMode::Stick)
	{
		if (bForward || bBack)
		{
			return MakeSlot(ThrowCommand, { MakeEnter(bForward ? GSTN_DirectionMasks::FacingForward : GSTN_DirectionMasks::FacingBack) }, Role);
		}
		if (bUp)
		{
			return WithLayout(MakeSlot(ThrowCommand, { MakePress({ GSTNCombatButtonTags::Button_Jump }) }, Role), { { TEXT("Link.ThrowJump"), &GSTNCombatButtonTags::Button_Jump } });
		}
		if (bDown)
		{
			return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Crouch }) }, Role + 1), { LinkCrouch });
		}
	}
	else if (Mode == EGSTN_LinkThrowInputMode::StickPlusSpecial)
	{
		if (bForward || bBack)
		{
			return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Special }), MakeTilted(bForward ? GSTN_DirectionMasks::FacingForward : GSTN_DirectionMasks::FacingBack) }, Role), { ThrowSpecial });
		}
		if (bUp)
		{
			return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Special }), MakeNeutralStick() }, Role), { ThrowSpecial });
		}
		if (bDown)
		{
			return WithLayout(MakeSlot(ThrowCommand, { MakePress({ Special }), MakeHold(Crouch) }, Role + 1), { ThrowSpecial, LinkCrouch });
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
	return WithLayout(MakeSlot(GSTNLinkTags::Command_Link_Grab_Special,
		{ MakePress({ GSTNCombatButtonTags::Button_Special }), MakeHold(GSTNCombatButtonTags::Button_Crouch) }, SpecialGrabPriorityOffset),
		{ { TEXT("Link.SpecialGrab"), &GSTNCombatButtonTags::Button_Special }, { TEXT("Link.Crouch"), &GSTNCombatButtonTags::Button_Crouch } });
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
	const TArray<FGameplayTag> Throws = GetThrowCommands(Fill.Mode, Fill.Ergonomics);
	// grab-link: a throw the mode does not use is hidden (its rows bound to nothing), so a kit's authored up throw cannot keep the strong press the
	// strong press of another plugin, nor a back throw the back tilt an instant F. answers
	const FGameplayTag AllThrows[] = { Command_Link_Throw_Forward, Command_Link_Throw_Back, Command_Link_Throw_Up, Command_Link_Throw_Down,
		Command_Link_Throw_Neutral, Command_Link_Throw_CrouchForward };
	for (const FGameplayTag& Command : AllThrows)
	{
		if (!Throws.Contains(Command))
		{
			for (FGSTN_BindingInput& Binding : Bindings)
			{
				if (Binding.Command == Command)
				{
					Binding.Ability.Reset();
				}
			}
		}
	}
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
				Binding.SlotOverride = SlotOrOverride(Fill.SlotOverrides, Command, [&Command, &Fill]() { return BuildThrowSlot(Command, Fill.Mode, Fill.Ergonomics); });
			}
		}
		++Bound;
	}

	if (Fill.Mode == EGSTN_LinkThrowInputMode::Notation)
	{
		// Owner 2026-11-23: no pummel ("lo leviamo, non ci serve"). Its rows are hidden (bound to nothing), so attack neutral is the N. throw's alone
		const FGameplayTag Pummel = Command_Link_Pummel;
		for (FGSTN_BindingInput& Binding : Bindings)
		{
			if (Binding.Command == Pummel)
			{
				Binding.Ability.Reset();
			}
		}
	}

	// grab-link: the dodge press sets the partner down, through the grabber's own Release ability (its release reason, the ring exit). Its own
	// command, a child of Release: a second Release row with the same requirement would be ambiguous to the loadout validator
	const FGameplayTag PutDown = Command_Link_Release_PutDown;
	if (Fill.bPutDownWithDodge && !Bindings.ContainsByPredicate([&PutDown](const FGSTN_BindingInput& Binding) { return Binding.Command == PutDown; }))
	{
		const FGameplayTag Release = Command_Link_Release;
		const FGameplayTag Grabber = Role_Grabber;
		const FGSTN_BindingInput* ReleaseRow = Bindings.FindByPredicate([&Release, &Grabber](const FGSTN_BindingInput& Binding)
		{
			return Binding.Command == Release && !Binding.Ability.IsNull() && Binding.Requirements.RequireTags.HasTagExact(Grabber);
		});
		if (ReleaseRow)
		{
			const FGSTN_BindingInput ReleaseCopy = *ReleaseRow;
			FGSTN_BindingInput& Added = Bindings.Add_GetRef(ReleaseCopy);
			Added.Command = PutDown;
			Added.bHasSlotOverride = true;
			Added.SlotOverride = SlotOrOverride(Fill.SlotOverrides, PutDown, []() { return BuildPutDownSlot(); });
		}
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
