#include "Tests/GSTN_TestUtils.h"
#include "Layout/GSTN_ButtonLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

// IO-2: the shipped layout (class defaults from DefaultGSTNInput.ini) gives today's buttons, and a function it does not name keeps the
// reader's default
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ButtonLayoutDefaultsTest, "GSTNInput.Core.Layout.Defaults", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ButtonLayoutDefaultsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Interact = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Interact"), false);
	const FGameplayTag Strong = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Strong"), false);
	const FGameplayTag Crouch = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Crouch"), false);
	const FGameplayTag Jump = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Jump"), false);
	if (!Interact.IsValid() || !Strong.IsValid() || !Crouch.IsValid() || !Jump.IsValid())
	{
		AddInfo(TEXT("Combat button tags not registered (GSTNCombat off): skipped"));
		return true;
	}
	const UGSTN_ButtonLayout& Layout = *GetDefault<UGSTN_ButtonLayout>();
	TestEqual(TEXT("Held.PickupDrop = B"), Layout.FindButton(TEXT("Held.PickupDrop")), Interact);
	TestEqual(TEXT("Held.Throw = RT"), Layout.FindButton(TEXT("Held.Throw")), Strong);
	TestEqual(TEXT("Held.KitSwap = B"), Layout.FindButton(TEXT("Held.KitSwap")), Interact);
	TestEqual(TEXT("Mount.Interact = B"), Layout.FindButton(TEXT("Mount.Interact")), Interact);
	TestEqual(TEXT("Mount.Crouch"), Layout.FindButton(TEXT("Mount.Crouch")), Crouch);
	TestEqual(TEXT("Spin.Drill"), Layout.FindButton(TEXT("Spin.Drill")), Crouch);
	TestEqual(TEXT("Drill.Exit"), Layout.FindButton(TEXT("Drill.Exit")), Jump);
	TestEqual(TEXT("Surface.LaunchBoost"), Layout.FindButton(TEXT("Surface.LaunchBoost")), Jump);
	TestFalse(TEXT("unknown function = empty"), Layout.FindButton(TEXT("Nobody.Names.This")).IsValid());
	TestEqual(TEXT("unknown function keeps the default"), UGSTN_ButtonLayout::GetButton(TEXT("Nobody.Names.This"), Strong), Strong);
	return true;
}

#endif
