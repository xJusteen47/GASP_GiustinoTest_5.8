// Spin and drill, ability-system half.

#include "GSTN_SpinDrillLibrary.h"

#include "Abilities/GSTN_GA_DrillDive.h"
#include "Abilities/GSTN_GA_Spin.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GSTN_SpinDrillKitExtension.h"
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
	// The cancels first, one for the ground and one for the air, so each requires one tag more than the spin it competes with (the ground
	// spin requires none, the air spin Mover.IsInAir). Equal counts would be an "arbitrary choice" binding conflict, which the loadout
	// compile reports as an error and refuses the whole kit (SD-05: no spin in the harness came from that)
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		const EGSTN_SpinSense Sense = Slot.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise) ? EGSTN_SpinSense::Clockwise : EGSTN_SpinSense::CounterClockwise;
		for (const bool bAir : { false, true })
		{
			FGSTN_AbilityBinding Cancel = MakeBinding(Slot, Sense == EGSTN_SpinSense::Clockwise ? UGSTN_GA_SpinCancelClockwise::StaticClass() : UGSTN_GA_SpinCancelCounterClockwise::StaticClass(), bAir);
			Cancel.Requirements.RequireTags.AddTag(UGSTN_GA_SpinCancel::RequiredSpinTag(Sense));
			Bindings.Add(MoveTemp(Cancel));
		}
	}
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		const bool bClockwise = Slot.Command.MatchesTag(GSTNSpinDrillTags::Command_Movement_Spin_Clockwise);
		FGSTN_AbilityBinding AirSpin = MakeBinding(Slot, bClockwise ? UGSTN_GA_AirSpinClockwise::StaticClass() : UGSTN_GA_AirSpinCounterClockwise::StaticClass(), true);
		// Once per airtime, at the command match: an ability block on the same tag would see the spend of the very frame that adopts the spin (SD-07)
		AirSpin.Requirements.IgnoreTags.AddTag(GSTNSpinDrillTags::State_AirSpinSpent);
		Bindings.Add(MoveTemp(AirSpin));
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

bool UGSTN_SpinDrillLibrary::AddSpinDrillExtensionToKit(UGSTN_PrimaryKitDefinition* Kit)
{
	if (!Kit || Kit->Extensions.Find<FGSTN_SpinDrillKitExtension>())
	{
		return false;
	}
	Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make<FGSTN_SpinDrillKitExtension>());
	return true;
}

int32 UGSTN_SpinDrillLibrary::RemoveSpinDrillExtensionFromKit(UGSTN_PrimaryKitDefinition* Kit)
{
	return Kit ? Kit->Extensions.ExtensionData.RemoveAll([](const FInstancedStruct& Entry) { return Entry.GetPtr<FGSTN_SpinDrillKitExtension>() != nullptr; }) : 0;
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
