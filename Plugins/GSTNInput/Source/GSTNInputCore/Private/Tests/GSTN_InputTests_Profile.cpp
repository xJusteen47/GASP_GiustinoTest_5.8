#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_InputProfile.h"
#include "Layout/GSTN_ButtonLayout.h"
#include "Algo/Reverse.h"

#if WITH_DEV_AUTOMATION_TESTS

// F1 of the rollback plan: the player's input profile (device, button overrides, picked commands) on top of the button layout, applied
// when a table is compiled. Pure, so the same layout and profile give the same slot on every machine.

namespace GSTNProfileTest
{
	using namespace GSTNTest;

	const FName SpecialFunction(TEXT("Test.Special"));
	const FName AttackFunction(TEXT("Test.Attack"));

	/** A layout with Test.Special remappable and two commands for the neutral special: Tap (a press) and DoubleTap (two presses) */
	TStrongObjectPtr<UGSTN_ButtonLayout> MakeLayout()
	{
		TStrongObjectPtr<UGSTN_ButtonLayout> Layout(NewObject<UGSTN_ButtonLayout>(GetTransientPackage()));
		Layout->Assignments.Reset();
		Layout->Assignments.Add({ SpecialFunction, Special() });
		Layout->Assignments.Add({ AttackFunction, Attack() });
		Layout->RemappableFunctions = { SpecialFunction };

		FGSTN_CommandChoiceSet Set;
		Set.VirtualCommand = GSTNTestTags::Special_Neutral;
		FGSTN_CommandChoice& Tap = Set.Choices.AddDefaulted_GetRef();
		Tap.Id = TEXT("Tap");
		Tap.Pattern = Pattern({ Press(Special()) });
		Tap.LayoutButtons.Add({ Special(), SpecialFunction });
		FGSTN_CommandChoice& DoubleTap = Set.Choices.AddDefaulted_GetRef();
		DoubleTap.Id = TEXT("DoubleTap");
		DoubleTap.Pattern = Pattern({ Mash(Special(), 2, 12) });
		DoubleTap.LayoutButtons.Add({ Special(), SpecialFunction });
		Set.GamepadDefault = NAME_None;
		Set.MouseKeyboardDefault = TEXT("DoubleTap");
		Layout->CommandChoices = { Set };
		return Layout;
	}

	/** The neutral special slot as a kit writes it: a press of the function's default button */
	FGSTN_InputSlotSpec MakeSpecialSlot()
	{
		FGSTN_InputSlotSpec Spec = Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) }));
		Spec.AddLayoutButton(SpecialFunction, Special());
		return Spec;
	}

	const UScriptStruct* FirstConditionType(const FGSTN_InputSlotSpec& Spec)
	{
		return Spec.Pattern.Steps.IsEmpty() || Spec.Pattern.Steps[0].AllOf.IsEmpty() ? nullptr : Spec.Pattern.Steps[0].AllOf[0].GetScriptStruct();
	}

	FGameplayTagContainer Buttons(const FGSTN_InputSlotSpec& Spec)
	{
		FGameplayTagContainer Out;
		for (const FGSTN_InputStep& Step : Spec.Pattern.Steps)
		{
			for (const TInstancedStruct<FGSTN_InputCondition>& Condition : Step.AllOf)
			{
				if (const FGSTN_InputCondition* Pointer = Condition.GetPtr())
				{
					Pointer->GetReferencedButtons(Out);
				}
			}
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputProfileSanitizeTest, "GSTNInput.Core.Profile.Sanitize", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputProfileSanitizeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNProfileTest;
	const TStrongObjectPtr<UGSTN_ButtonLayout> Layout = MakeLayout();

	FGSTN_InputProfile Raw;
	Raw.Device = EGSTN_InputDevice::MouseKeyboard;
	Raw.ButtonOverrides.Add({ AttackFunction, Strong() });                       // not remappable: dropped
	Raw.ButtonOverrides.Add({ SpecialFunction, Strong() });                      // kept
	Raw.ButtonOverrides.Add({ SpecialFunction, Jump() });                        // second entry of the same function: dropped
	Raw.ButtonOverrides.Add({ SpecialFunction, GSTNTestTags::Special_Neutral }); // not a button (and a duplicate): dropped
	Raw.CommandPicks.Add({ GSTNTestTags::Special_Neutral, TEXT("Nope") });       // not offered: dropped
	Raw.CommandPicks.Add({ GSTNTestTags::Attack_Neutral, TEXT("Tap") });         // command not remappable: dropped
	Raw.CommandPicks.Add({ GSTNTestTags::Special_Neutral, TEXT("Tap") });        // kept
	for (int32 Index = 0; Index < 100; ++Index)
	{
		Raw.ButtonOverrides.Add({ SpecialFunction, Attack() }); // a flood is cut, never kept
	}

	const FGSTN_InputProfile Clean = Raw.Sanitized(*Layout);
	TestEqual(TEXT("device kept"), Clean.Device, EGSTN_InputDevice::MouseKeyboard);
	TestEqual(TEXT("one override"), Clean.ButtonOverrides.Num(), 1);
	TestEqual(TEXT("the remappable one"), Clean.FindOverride(SpecialFunction), Strong());
	TestFalse(TEXT("no override of a fixed function"), Clean.FindOverride(AttackFunction).IsValid());
	TestEqual(TEXT("one pick"), Clean.CommandPicks.Num(), 1);
	TestEqual(TEXT("the offered pick"), Clean.FindPick(GSTNTestTags::Special_Neutral), FName(TEXT("Tap")));
	TestTrue(TEXT("sanitizing twice changes nothing"), Clean.Sanitized(*Layout) == Clean);

	// The order the player made the choices in does not change the bytes
	FGSTN_InputProfile Reordered = Raw;
	Algo::Reverse(Reordered.CommandPicks);
	TestTrue(TEXT("order independent picks"), Reordered.Sanitized(*Layout).CommandPicks == Clean.CommandPicks);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputProfileApplyTest, "GSTNInput.Core.Profile.Apply", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputProfileApplyTest::RunTest(const FString& Parameters)
{
	using namespace GSTNProfileTest;
	const TStrongObjectPtr<UGSTN_ButtonLayout> Layout = MakeLayout();

	// Gamepad, no choices: the kit slot's own press, on the layout's button
	{
		FGSTN_InputSlotSpec Spec = MakeSpecialSlot();
		Spec.ApplyPlayerInput(*Layout, FGSTN_InputProfile());
		TestEqual(TEXT("pad default: a press"), FirstConditionType(Spec), FGSTN_InputCondition_Press::StaticStruct());
		TestTrue(TEXT("pad default: on Special"), Buttons(Spec).HasTagExact(Special()));
		TestTrue(TEXT("layout buttons consumed"), Spec.LayoutButtons.IsEmpty());
	}

	// Mouse and keyboard, no pick: the device's default command (a double tap)
	{
		FGSTN_InputProfile Profile;
		Profile.Device = EGSTN_InputDevice::MouseKeyboard;
		FGSTN_InputSlotSpec Spec = MakeSpecialSlot();
		Spec.ApplyPlayerInput(*Layout, Profile.Sanitized(*Layout));
		TestEqual(TEXT("keyboard default: a double tap"), FirstConditionType(Spec), FGSTN_InputCondition_Mash::StaticStruct());
	}

	// Mouse and keyboard with a pick: the pick wins over the device default
	{
		FGSTN_InputProfile Profile;
		Profile.Device = EGSTN_InputDevice::MouseKeyboard;
		Profile.CommandPicks.Add({ GSTNTestTags::Special_Neutral, TEXT("Tap") });
		FGSTN_InputSlotSpec Spec = MakeSpecialSlot();
		Spec.ApplyPlayerInput(*Layout, Profile.Sanitized(*Layout));
		TestEqual(TEXT("pick: a press"), FirstConditionType(Spec), FGSTN_InputCondition_Press::StaticStruct());
	}

	// The override of a remappable function moves the button, also inside a picked command
	{
		FGSTN_InputProfile Profile;
		Profile.ButtonOverrides.Add({ SpecialFunction, Strong() });
		Profile.CommandPicks.Add({ GSTNTestTags::Special_Neutral, TEXT("DoubleTap") });
		FGSTN_InputSlotSpec Spec = MakeSpecialSlot();
		Spec.ApplyPlayerInput(*Layout, Profile.Sanitized(*Layout));
		TestEqual(TEXT("override: a double tap"), FirstConditionType(Spec), FGSTN_InputCondition_Mash::StaticStruct());
		TestTrue(TEXT("override: on Strong"), Buttons(Spec).HasTagExact(Strong()));
		TestFalse(TEXT("override: not on Special any more"), Buttons(Spec).HasTagExact(Special()));
	}

	// A fixed function ignores an override even when the profile was not sanitized
	{
		FGSTN_InputProfile Profile;
		Profile.ButtonOverrides.Add({ AttackFunction, Strong() });
		FGSTN_InputSlotSpec Spec = Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) }));
		Spec.AddLayoutButton(AttackFunction, Attack());
		Spec.ApplyPlayerInput(*Layout, Profile);
		TestTrue(TEXT("fixed function stays on Attack"), Buttons(Spec).HasTagExact(Attack()));
	}

	// Applying twice changes nothing (compilers may call it on a slot already applied)
	{
		FGSTN_InputProfile Profile;
		Profile.ButtonOverrides.Add({ SpecialFunction, Strong() });
		FGSTN_InputSlotSpec Once = MakeSpecialSlot();
		Once.ApplyPlayerInput(*Layout, Profile);
		FGSTN_InputSlotSpec Twice = Once;
		Twice.ApplyPlayerInput(*Layout, Profile);
		TestTrue(TEXT("idempotent"), Buttons(Once) == Buttons(Twice) && FirstConditionType(Once) == FirstConditionType(Twice));
	}
	return true;
}

#endif
