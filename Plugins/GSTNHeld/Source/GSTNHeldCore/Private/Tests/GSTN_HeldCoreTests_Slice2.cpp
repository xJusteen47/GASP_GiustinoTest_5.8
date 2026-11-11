// Held objects, pure core tests of slice 2 (GSTNHeld.Core.*): break rules, throw aim, default slots.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldBreakRulesTest, "GSTNHeld.Core.BreakRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldBreakRulesTest::RunTest(const FString& Parameters)
{
	// Tags by name, as the ini names them: no dependency on the plugins that project them
	const FGameplayTag Hitstun = FGameplayTag::RequestGameplayTag(TEXT("Combat.State.Hitstun"), false);
	const FGameplayTag Busy = FGameplayTag::RequestGameplayTag(TEXT("Combat.State.Busy"), false);
	const FGameplayTag KO = FGameplayTag::RequestGameplayTag(TEXT("Combat.Event.KO"), false);
	if (!TestTrue(TEXT("combat tags registered"), Hitstun.IsValid() && Busy.IsValid() && KO.IsValid()))
	{
		return false;
	}
	FGSTN_HeldBreakRules Rules;
	Rules.DropWhenHolderHasAny.AddTag(Hitstun);
	Rules.DropOnHolderEvents.AddTag(KO);

	FGameplayTagContainer Holder;
	Holder.AddTag(Busy);
	TestFalse(TEXT("busy holder keeps it"), FGSTN_HeldRules::BreaksOnState(Holder, Rules));
	Holder.AddTag(Hitstun);
	TestTrue(TEXT("hit holder drops it"), FGSTN_HeldRules::BreaksOnState(Holder, Rules));
	TestFalse(TEXT("empty rules never break"), FGSTN_HeldRules::BreaksOnState(Holder, FGSTN_HeldBreakRules()));
	TestTrue(TEXT("KO event breaks"), FGSTN_HeldRules::BreaksOnEvent(KO, Rules));
	TestFalse(TEXT("other event does not"), FGSTN_HeldRules::BreaksOnEvent(Busy, Rules));
	TestFalse(TEXT("invalid event does not"), FGSTN_HeldRules::BreaksOnEvent(FGameplayTag(), Rules));

	// The project defaults from DefaultGSTNHeld.ini: hitstun and KO at least
	const FGSTN_HeldBreakRules& Defaults = UGSTN_HeldSettings::Get().DefaultBreakRules;
	TestTrue(TEXT("ini: hitstun drops"), Defaults.DropWhenHolderHasAny.HasTagExact(Hitstun));
	TestTrue(TEXT("ini: KO drops"), Defaults.DropOnHolderEvents.HasTagExact(KO));

	// Drop point: in front of the holder by DropForwardCm, at its height
	Rules.DropForwardCm = 60.f;
	const FTransform HolderTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 50.0));
	TestTrue(TEXT("drop point in front"), FGSTN_HeldRules::DropPoint(HolderTransform, Rules).Equals(FVector(100.0, 60.0, 50.0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldThrowAimTest, "GSTNHeld.Core.ThrowAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldThrowAimTest::RunTest(const FString& Parameters)
{
	FGSTN_HeldThrowInput Input;
	Input.AimHoldFrames = 12;
	Input.bCameraAimWhenReticleOn = true;
	Input.AimView = EGSTN_HeldAimView::KeepView;
	TestEqual(TEXT("tap = stick"), FGSTN_HeldRules::ChooseThrowAim(1, false, Input), EGSTN_HeldThrowAim::Stick);
	TestEqual(TEXT("just under the hold = stick"), FGSTN_HeldRules::ChooseThrowAim(11, false, Input), EGSTN_HeldThrowAim::Stick);
	TestEqual(TEXT("held = camera"), FGSTN_HeldRules::ChooseThrowAim(12, false, Input), EGSTN_HeldThrowAim::Camera);
	TestEqual(TEXT("tap with the reticle on = camera"), FGSTN_HeldRules::ChooseThrowAim(1, true, Input), EGSTN_HeldThrowAim::Camera);
	Input.bCameraAimWhenReticleOn = false;
	TestEqual(TEXT("tap with the reticle on, rule off = stick"), FGSTN_HeldRules::ChooseThrowAim(1, true, Input), EGSTN_HeldThrowAim::Stick);
	// M10 G7 shoulder aim: held = the shoulder aim; a reticle tap stays a camera aim (the view never went over the shoulder)
	Input.AimView = EGSTN_HeldAimView::Shoulder;
	Input.bCameraAimWhenReticleOn = true;
	TestEqual(TEXT("held with the shoulder view = shoulder"), FGSTN_HeldRules::ChooseThrowAim(12, false, Input), EGSTN_HeldThrowAim::Shoulder);
	TestEqual(TEXT("tap with the reticle on and the shoulder view = camera"), FGSTN_HeldRules::ChooseThrowAim(1, true, Input), EGSTN_HeldThrowAim::Camera);
	TestEqual(TEXT("tap with the shoulder view = stick"), FGSTN_HeldRules::ChooseThrowAim(1, false, Input), EGSTN_HeldThrowAim::Stick);
	TestFalse(TEXT("not aiming before the hold"), FGSTN_HeldRules::IsAiming(11, Input));
	TestTrue(TEXT("aiming from the hold"), FGSTN_HeldRules::IsAiming(12, Input));
	TestTrue(TEXT("ini values sane"), UGSTN_HeldSettings::Get().ThrowInput.AimHoldFrames > 0
		&& UGSTN_HeldSettings::Get().ThrowInput.MaxAimFrames > UGSTN_HeldSettings::Get().ThrowInput.AimHoldFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldSlotsTest, "GSTNHeld.Core.DefaultSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldSlotsTest::RunTest(const FString& Parameters)
{
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_HeldSlots::Build();
	const TArray<FGSTN_HeldSlotBinding> Bindings = UGSTN_HeldSlots::DefaultBindings();
	if (!TestEqual(TEXT("four slots"), Slots.Num(), 4) || !TestEqual(TEXT("four bindings"), Bindings.Num(), 4))
	{
		return false;
	}
	const FGameplayTag Carrying = GSTNHeldTags::Held_Carrying;
	TestEqual(TEXT("pickup command"), Slots[0].Command, GSTNHeldTags::Command_Held_Pickup.GetTag());
	TestEqual(TEXT("drop command"), Slots[1].Command, GSTNHeldTags::Command_Held_Drop.GetTag());
	TestEqual(TEXT("throw command"), Slots[2].Command, GSTNHeldTags::Command_Held_Throw.GetTag());
	TestTrue(TEXT("pickup below the grab"), Slots[0].PriorityOffset < 0);
	TestTrue(TEXT("drop and throw above the attacks"), Slots[1].PriorityOffset > 0 && Slots[2].PriorityOffset > 0);
	for (const FGSTN_InputSlotSpec& Slot : Slots)
	{
		TestTrue(TEXT("falls through"), Slot.bFallThroughWhenUnavailable);
		TestEqual(TEXT("one step"), Slot.Pattern.Steps.Num(), 1);
	}
	TestTrue(TEXT("pickup ignores carrying"), Bindings[0].Requirements.IgnoreTags.HasTagExact(Carrying));
	TestTrue(TEXT("pickup requires an object in reach (slice 3)"), Bindings[0].Requirements.RequireTags.HasTagExact(GSTNHeldTags::Interact_Target_Pickup)
		&& Bindings[0].Requirements.RequireTags.Num() == 1);
	TestTrue(TEXT("drop requires carrying"), Bindings[1].Requirements.RequireTags.HasTagExact(Carrying));
	TestTrue(TEXT("throw requires carrying"), Bindings[2].Requirements.RequireTags.HasTagExact(Carrying));
	return true;
}

#endif
