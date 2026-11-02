// Spin and drill, ability-system half.

#include "GSTN_SpinDrillLibrary.h"

#include "Abilities/GSTN_GA_DrillDive.h"
#include "Abilities/GSTN_GA_Spin.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GSTN_SpinDrillTags.h"
#include "Input/GSTN_SpinSlots.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "MoverTypes.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"

namespace GSTNSpinDrillLibrary
{
	FGSTN_AbilityBinding MakeBinding(const FGSTN_InputSlotSpec& Slot, TSubclassOf<UGameplayAbility> Ability, bool bAir)
	{
		FGSTN_AbilityBinding Binding;
		Binding.Command = Slot.Command;
		Binding.SlotOverride.Inline = Slot;
		Binding.Ability = TSoftClassPtr<UGameplayAbility>(Ability.Get());
		if (bAir)
		{
			Binding.Requirements.RequireTags.AddTag(Mover_IsInAir);
		}
		else
		{
			Binding.Requirements.IgnoreTags.AddTag(Mover_IsInAir);
		}
		return Binding;
	}

	bool IsSpinCommand(const FGameplayTag& Command)
	{
		return Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise) || Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_CounterClockwise)
			|| Command.MatchesTagExact(GSTNSpinDrillTags::Command_Movement_Spin_Button) || Command.MatchesTagExact(GSTNSpinDrillTags::Command_Movement_Drill);
	}

	const FGSTN_DrillFragment* ShownFragment(const AActor* Actor)
	{
		const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
		const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
		return Sync ? FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Sync->Fragments) : nullptr;
	}
}

TArray<FGSTN_AbilityBinding> UGSTN_SpinDrillLibrary::MakeDefaultSpinBindings(bool bKeyboardShortcut)
{
	using namespace GSTNSpinDrillLibrary;
	TArray<FGSTN_AbilityBinding> Bindings;
	const TArray<FGSTN_InputSlotSpec> Slots = FGSTN_SpinSlots::Build(bKeyboardShortcut);
	// The cancels first: in the air a cancel (requires the spin's sense tag) and the air spin (requires Mover.IsInAir) both require one
	// tag, and the first of equally specific bindings wins. On the ground the cancel requires more than the ground spin and wins anyway
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		const EGSTN_SpinSense Sense = Slot.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise) ? EGSTN_SpinSense::Clockwise : EGSTN_SpinSense::CounterClockwise;
		FGSTN_AbilityBinding Cancel;
		Cancel.Command = Slot.Command;
		Cancel.SlotOverride.Inline = Slot;
		Cancel.Ability = TSoftClassPtr<UGameplayAbility>(Sense == EGSTN_SpinSense::Clockwise ? UGSTN_GA_SpinCancelClockwise::StaticClass() : UGSTN_GA_SpinCancelCounterClockwise::StaticClass());
		Cancel.Requirements.RequireTags.AddTag(UGSTN_GA_SpinCancel::RequiredSpinTag(Sense));
		Bindings.Add(MoveTemp(Cancel));
	}
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		const bool bClockwise = Slot.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise);
		Bindings.Add(MakeBinding(Slot, bClockwise ? UGSTN_GA_AirSpinClockwise::StaticClass() : UGSTN_GA_AirSpinCounterClockwise::StaticClass(), true));
		Bindings.Add(MakeBinding(Slot, bClockwise ? UGSTN_GA_SpinClockwise::StaticClass() : UGSTN_GA_SpinCounterClockwise::StaticClass(), false));
	}
	// D8: crouch pressed during a spin, on the ground or in the air
	FGSTN_AbilityBinding Drill;
	Drill.Command = GSTNSpinDrillTags::Command_Movement_Drill.GetTag();
	Drill.SlotOverride.Inline = FGSTN_SpinSlots::MakeDrillSlot();
	Drill.Ability = TSoftClassPtr<UGameplayAbility>(UGSTN_GA_DrillDive::StaticClass());
	Drill.Requirements.RequireTags.AddTag(GSTNSpinDrillTags::State_Spinning);
	Bindings.Add(MoveTemp(Drill));
	return Bindings;
}

int32 UGSTN_SpinDrillLibrary::AddSpinBindingsToKit(UGSTN_PrimaryKitDefinition* Kit, bool bKeyboardShortcut)
{
	if (!Kit)
	{
		return 0;
	}
	int32 Added = 0;
	for (const FGSTN_AbilityBinding& Binding : MakeDefaultSpinBindings(bKeyboardShortcut))
	{
		if (!Kit->KitBindings.ContainsByPredicate([&Binding](const FGSTN_AbilityBinding& Existing) { return Existing.HasSameKey(Binding); }))
		{
			Kit->KitBindings.Add(Binding);
			++Added;
		}
	}
	return Added;
}

int32 UGSTN_SpinDrillLibrary::RemoveSpinBindingsFromKit(UGSTN_PrimaryKitDefinition* Kit)
{
	return Kit ? Kit->KitBindings.RemoveAll([](const FGSTN_AbilityBinding& Binding) { return GSTNSpinDrillLibrary::IsSpinCommand(Binding.Command); }) : 0;
}

FString UGSTN_SpinDrillLibrary::GetShownDrillText(const AActor* Actor)
{
	const FGSTN_DrillFragment* Fragment = GSTNSpinDrillLibrary::ShownFragment(Actor);
	if (!Fragment)
	{
		return FString();
	}
	TAnsiStringBuilder<128> Text;
	Fragment->ToString(Text);
	return FString(Text.ToString());
}

bool UGSTN_SpinDrillLibrary::GetShownDrillNumbers(const AActor* Actor, int32& OutPhase, int32& OutPhaseStartFrame, int32& OutAirSpinsUsed, int32& OutLowGravityUntilFrame)
{
	const FGSTN_DrillFragment* Fragment = GSTNSpinDrillLibrary::ShownFragment(Actor);
	OutPhase = Fragment ? static_cast<int32>(Fragment->Phase) : 0;
	OutPhaseStartFrame = Fragment ? Fragment->PhaseStartFrame : INDEX_NONE;
	OutAirSpinsUsed = Fragment ? Fragment->AirSpinsUsed : 0;
	OutLowGravityUntilFrame = Fragment ? Fragment->LowGravityUntilFrame : INDEX_NONE;
	return Fragment != nullptr;
}

bool UGSTN_SpinDrillLibrary::HasSpinLowGravity(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return ASC && ASC->HasMatchingGameplayTag(GSTNSpinDrillTags::State_SpinLowGravity);
}
