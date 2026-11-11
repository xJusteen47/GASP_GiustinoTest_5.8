// Held objects, pure core tests of M11 K1 (GSTNHeld.Core.KitSwap*): the kit swap slot, its binding and its settings.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldKitSwapSlotTest, "GSTNHeld.Core.KitSwapSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldKitSwapSlotTest::RunTest(const FString& Parameters)
{
	// The slot: a release of the grab button after the hold, never a press (the press stays the grab's and the pickup's)
	const FGSTN_InputSlotSpec Slot = UGSTN_HeldSlots::BuildKitSwapSlot(31);
	TestEqual(TEXT("command"), Slot.Command, GSTNHeldTags::Command_Kit_Swap.GetTag());
	TestTrue(TEXT("falls through with no kit framed"), Slot.bFallThroughWhenUnavailable);
	TestEqual(TEXT("own offset on the interact button"), Slot.PriorityOffset, UGSTN_HeldSlots::KitSwapSlotPriorityOffset);
	TestTrue(TEXT("offset shared with no held slot"), Slot.PriorityOffset != UGSTN_HeldSlots::PickupSlotPriorityOffset
		&& Slot.PriorityOffset != UGSTN_HeldSlots::CarrySlotPriorityOffset);
	if (!TestEqual(TEXT("one step"), Slot.Pattern.Steps.Num(), 1) || !TestEqual(TEXT("one condition"), Slot.Pattern.Steps[0].AllOf.Num(), 1))
	{
		return false;
	}
	const FGSTN_InputCondition_Release* Release = Slot.Pattern.Steps[0].AllOf[0].GetPtr<FGSTN_InputCondition_Release>();
	if (!TestNotNull(TEXT("a release"), Release))
	{
		return false;
	}
	TestEqual(TEXT("of the grab button"), Release->Button, GSTNCombatButtonTags::Button_Interact.GetTag());
	TestEqual(TEXT("after the hold"), Release->MinHeldFrames, 31);
	TestEqual(TEXT("no upper bound"), Release->MaxHeldFrames, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("at least one frame"), UGSTN_HeldSlots::BuildKitSwapSlot(0).Pattern.Steps[0].AllOf[0].GetPtr<FGSTN_InputCondition_Release>()->MinHeldFrames, 1);

	// The binding: only with a kit framed, never while carrying (B is the drop then)
	const FGSTN_TagRequirements Requirements = UGSTN_HeldSlots::KitSwapRequirements();
	TestTrue(TEXT("requires a framed kit"), Requirements.RequireTags.HasTagExact(GSTNHeldTags::Interact_Target_Kit) && Requirements.RequireTags.Num() == 1);
	TestTrue(TEXT("ignores carrying"), Requirements.IgnoreTags.HasTagExact(GSTNHeldTags::Held_Carrying)
		&& Requirements.IgnoreTags.HasTagExact(GSTNHeldTags::Held_CarryingEnemy));

	// Appended once, never over a kit's own binding, never without an ability
	const FSoftObjectPath Ability(TEXT("/Script/GSTNHeldRuntime.GSTN_GA_KitSwap"));
	TArray<FGSTN_BindingInput> Bindings;
	TestTrue(TEXT("added"), UGSTN_HeldSlots::AppendKitSwapBinding(Ability, 31, Bindings));
	TestFalse(TEXT("not twice"), UGSTN_HeldSlots::AppendKitSwapBinding(Ability, 31, Bindings));
	if (TestEqual(TEXT("one binding"), Bindings.Num(), 1))
	{
		TestTrue(TEXT("slot override"), Bindings[0].bHasSlotOverride && Bindings[0].SlotOverride.Command == GSTNHeldTags::Command_Kit_Swap.GetTag());
		TestEqual(TEXT("ability"), Bindings[0].Ability, Ability);
	}
	TArray<FGSTN_BindingInput> Own;
	Own.AddDefaulted_GetRef().Command = GSTNHeldTags::Command_Kit_Swap;
	TestFalse(TEXT("a kit's own swap binding wins"), UGSTN_HeldSlots::AppendKitSwapBinding(Ability, 31, Own));
	TArray<FGSTN_BindingInput> None;
	TestFalse(TEXT("no ability, no binding"), UGSTN_HeldSlots::AppendKitSwapBinding(FSoftObjectPath(), 31, None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldKitSwapSettingsTest, "GSTNHeld.Core.KitSwapSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldKitSwapSettingsTest::RunTest(const FString& Parameters)
{
	FGSTN_KitSwapTuning Tuning;
	TestEqual(TEXT("0.5 s = 31 frames at 62.5 Hz"), Tuning.GetHoldFrames(), 31);
	Tuning.HoldSeconds = 0.f;
	TestEqual(TEXT("never a tap"), Tuning.GetHoldFrames(), 2);

	// The owner's decisions in DefaultGSTNHeld.ini (2026-11-11): on, half a second, the framed kit, the kit left on a KO
	const FGSTN_KitSwapTuning& Project = UGSTN_HeldSettings::Get().KitSwap;
	TestTrue(TEXT("on"), Project.bEnabled);
	TestEqual(TEXT("half a second"), Project.HoldSeconds, 0.5f);
	TestTrue(TEXT("longer than the movement set's hold (12 frames)"), Project.GetHoldFrames() > 12);
	TestTrue(TEXT("the framed kit"), Project.bTakesFocusedKit);
	TestEqual(TEXT("a KO leaves the kit"), Project.DeathDrop, EGSTN_KitDeathDrop::DropAndLose);
	TestTrue(TEXT("respawn kit read from the ini"), Project.RespawnKit.IsValid());
	const int32* Tier = UGSTN_HeldSettings::Get().CommandTiers.Find(FGameplayTag::RequestGameplayTag(TEXT("Command.Kit"), false));
	TestTrue(TEXT("Command.Kit tier as the held commands"), Tier && *Tier == 360);
	return true;
}

#endif
