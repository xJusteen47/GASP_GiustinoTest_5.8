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
	// IO-2b: shield, standard and movement sets, link grabs (same buttons as before the layout)
	const TPair<const TCHAR*, const TCHAR*> Rows[] = {
		{ TEXT("Defence.Shield"), TEXT("Input.Button.Shield") },
		{ TEXT("Combat.Attack"), TEXT("Input.Button.Attack") },
		{ TEXT("Combat.Strong"), TEXT("Input.Button.Strong") },
		{ TEXT("Combat.Special"), TEXT("Input.Button.Special") },
		{ TEXT("Combat.Crouch"), TEXT("Input.Button.Crouch") },
		{ TEXT("Movement.Dash"), TEXT("Input.Button.Sprint") },
		{ TEXT("Movement.Dodge"), TEXT("Input.Button.Dodge") },
		{ TEXT("Movement.Interact"), TEXT("Input.Button.Interact") },
		{ TEXT("Link.Grab"), TEXT("Input.Button.Interact") },
		{ TEXT("Link.Release"), TEXT("Input.Button.Interact") },
		{ TEXT("Link.Pummel"), TEXT("Input.Button.Attack") },
		{ TEXT("Link.ThrowAttack"), TEXT("Input.Button.Attack") },
		{ TEXT("Link.ThrowStrong"), TEXT("Input.Button.Strong") },
		{ TEXT("Link.ThrowSpecial"), TEXT("Input.Button.Special") },
		{ TEXT("Link.ThrowJump"), TEXT("Input.Button.Jump") },
		{ TEXT("Link.Crouch"), TEXT("Input.Button.Crouch") },
		{ TEXT("Link.LatchAttack"), TEXT("Input.Button.Attack") },
		{ TEXT("Link.SpecialGrab"), TEXT("Input.Button.Special") },
	};
	for (const TPair<const TCHAR*, const TCHAR*>& Row : Rows)
	{
		const FGameplayTag Expected = FGameplayTag::RequestGameplayTag(Row.Value, false);
		TestTrue(FString::Printf(TEXT("%s = %s"), Row.Key, Row.Value), Expected.IsValid() && Layout.FindButton(Row.Key) == Expected);
	}

	TestFalse(TEXT("unknown function = empty"), Layout.FindButton(TEXT("Nobody.Names.This")).IsValid());
	TestEqual(TEXT("unknown function keeps the default"), UGSTN_ButtonLayout::GetButton(TEXT("Nobody.Names.This"), Strong), Strong);
	return true;
}

#endif
