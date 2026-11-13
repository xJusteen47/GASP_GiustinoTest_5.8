// Held objects, pure core tests of M11 slice 3b (GSTNHeld.Core.SlotInputs): the held and kit swap buttons come from data, not code.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameplayTag PressButtonOf(const FGSTN_InputSlotSpec& Slot)
	{
		const FGSTN_InputCondition_Press* Press = Slot.Pattern.Steps.Num() > 0 && Slot.Pattern.Steps[0].AllOf.Num() > 0
			? Slot.Pattern.Steps[0].AllOf[0].GetPtr<FGSTN_InputCondition_Press>() : nullptr;
		return Press ? Press->Buttons.First() : FGameplayTag();
	}

	FGameplayTag ReleaseButtonOf(const FGSTN_InputSlotSpec& Slot)
	{
		const FGSTN_InputCondition_Release* Release = Slot.Pattern.Steps.Num() > 0 && Slot.Pattern.Steps[0].AllOf.Num() > 0
			? Slot.Pattern.Steps[0].AllOf[0].GetPtr<FGSTN_InputCondition_Release>() : nullptr;
		return Release ? Release->Button : FGameplayTag();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldSlotInputsTest, "GSTNHeld.Core.SlotInputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldSlotInputsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Interact = GSTNCombatButtonTags::Button_Interact.GetTag();
	const FGameplayTag Strong = GSTNCombatButtonTags::Button_Strong.GetTag();
	const FGameplayTag Attack = GSTNCombatButtonTags::Button_Attack.GetTag();
	const FGameplayTag Special = GSTNCombatButtonTags::Button_Special.GetTag();

	// Empty inputs = the shipped design (B pickup/drop/swap, RT throws)
	const TArray<FGSTN_InputSlotSpec> Defaults = UGSTN_HeldSlots::Build();
	if (!TestEqual(TEXT("four slots"), Defaults.Num(), 4))
	{
		return false;
	}
	TestEqual(TEXT("pickup on B"), PressButtonOf(Defaults[0]), Interact);
	TestEqual(TEXT("drop on B"), PressButtonOf(Defaults[1]), Interact);
	TestEqual(TEXT("throw on RT"), PressButtonOf(Defaults[2]), Strong);
	TestEqual(TEXT("enemy throw on RT"), PressButtonOf(Defaults[3]), Strong);
	TestEqual(TEXT("swap on B"), ReleaseButtonOf(UGSTN_HeldSlots::BuildKitSwapSlot(31)), Interact);

	// The project's settings reproduce the same buttons (DefaultGSTNHeld.ini names them; empty would fall back to the same)
	const FGSTN_HeldSlotInputs FromSettings = FGSTN_HeldSlotInputs::FromSettings();
	TestEqual(TEXT("settings pickup/drop = B"), FromSettings.GetPickupDropButton(), Interact);
	TestEqual(TEXT("settings throw = RT"), FromSettings.GetThrowButton(), Strong);
	TestEqual(TEXT("settings swap = B"), FromSettings.GetKitSwapButton(), Interact);

	// Other buttons chosen as data
	FGSTN_HeldSlotInputs Moved;
	Moved.PickupDropButton = Attack;
	Moved.ThrowButton = Special;
	Moved.KitSwapButton = Special;
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_HeldSlots::Build(Moved);
	TestEqual(TEXT("pickup moved"), PressButtonOf(Slots[0]), Attack);
	TestEqual(TEXT("drop moved"), PressButtonOf(Slots[1]), Attack);
	TestEqual(TEXT("throw moved"), PressButtonOf(Slots[2]), Special);
	TestEqual(TEXT("enemy throw moved"), PressButtonOf(Slots[3]), Special);
	TestEqual(TEXT("offsets kept"), Slots[0].PriorityOffset, UGSTN_HeldSlots::PickupSlotPriorityOffset);

	// A whole slot replaced by an override, in the built set and in the kit bindings
	TArray<FGSTN_InputSlotSpec> Overrides;
	FGSTN_InputSlotSpec& DropOverride = Overrides.AddDefaulted_GetRef();
	DropOverride.Command = GSTNHeldTags::Command_Held_Drop;
	DropOverride.PriorityOffset = 7;
	FGSTN_InputSlotSpec& SwapOverride = Overrides.AddDefaulted_GetRef();
	SwapOverride.Command = GSTNHeldTags::Command_Kit_Swap;
	SwapOverride.PriorityOffset = -9;
	FGSTN_HeldSlotInputs WithOverrides;
	WithOverrides.SlotOverrides = &Overrides;
	TestEqual(TEXT("override replaces the drop"), UGSTN_HeldSlots::Build(WithOverrides)[1].PriorityOffset, 7);
	TestEqual(TEXT("the others stay built"), PressButtonOf(UGSTN_HeldSlots::Build(WithOverrides)[0]), Interact);

	TArray<FGSTN_BindingInput> Bindings;
	const FSoftObjectPath Ability(TEXT("/Script/GSTNHeldRuntime.GSTN_GA_HeldDrop"));
	UGSTN_HeldSlots::AppendKitBindings(Ability, Ability, Ability, Bindings, WithOverrides);
	const FGSTN_BindingInput* Drop = Bindings.FindByPredicate([](const FGSTN_BindingInput& B) { return B.Command == GSTNHeldTags::Command_Held_Drop.GetTag(); });
	TestTrue(TEXT("kit drop binding carries the override"), Drop && Drop->SlotOverride.PriorityOffset == 7);

	TArray<FGSTN_BindingInput> Swap;
	TestTrue(TEXT("swap added"), UGSTN_HeldSlots::AppendKitSwapBinding(Ability, 31, Swap, WithOverrides));
	TestTrue(TEXT("swap binding carries the override"), Swap.Num() == 1 && Swap[0].SlotOverride.PriorityOffset == -9);
	TArray<FGSTN_BindingInput> SwapMoved;
	UGSTN_HeldSlots::AppendKitSwapBinding(Ability, 31, SwapMoved, Moved);
	TestTrue(TEXT("swap on the chosen button"), SwapMoved.Num() == 1 && ReleaseButtonOf(SwapMoved[0].SlotOverride) == Special);
	return true;
}

#endif
