#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_StandardSlots.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNCombatTest;

namespace
{
	/** The command the standard set triggers for a history, empty if none */
	FGameplayTag Trigger(const TArray<FGSTN_InputSlotSpec>& Slots, const FGSTN_InputHistory& History, const UGSTN_InputSettings& Settings)
	{
		const FGSTN_SlotResolution Resolution = ResolveSlots(Slots, History, Settings);
		return Resolution.SlotIndex == INDEX_NONE ? FGameplayTag() : Slots[Resolution.SlotIndex].Command;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsShapeTest, "GSTNCombat.Core.StandardSlots.Shapes", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsShapeTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();
	TestEqual(TEXT("12 family commands and 4 chord commands"), Slots.Num(), 16);

	struct FFamily
	{
		const TCHAR* Button;
		const FNativeGameplayTag& Neutral;
		const FNativeGameplayTag& Directional;
		const FNativeGameplayTag& CrouchNeutral;
		const FNativeGameplayTag& CrouchDirectional;
	};
	using namespace GSTNCommandTags;
	const FFamily Families[] = {
		{ TEXT("A"), Attack_Neutral, Attack_Directional, Attack_CrouchNeutral, Attack_CrouchDirectional },
		{ TEXT("B"), Strong_Neutral, Strong_Directional, Strong_CrouchNeutral, Strong_CrouchDirectional },
		{ TEXT("C"), Special_Neutral, Special_Directional, Special_CrouchNeutral, Special_CrouchDirectional },
	};

	// Camera and character turned or not: the results must not change.
	const TCHAR* Orientations[] = { TEXT(""), TEXT("0:cam=90 0:face=90 "), TEXT("0:cam=200 0:face=20 ") };

	for (const FFamily& Family : Families)
	{
		for (const TCHAR* Orientation : Orientations)
		{
			const auto Expect = [&](const TCHAR* What, const FString& Notation, const FNativeGameplayTag& Expected)
			{
				const FGSTN_InputHistory History = PlayTo(*this, *(FString(Orientation) + Notation), *Settings);
				TestEqual(*FString::Printf(TEXT("%s, button %s, orientation \"%s\""), What, Family.Button, Orientation),
					Trigger(Slots, History, *Settings).ToString(), Expected.GetTag().ToString());
			};
			const FString Press = FString::Printf(TEXT("5:+%s"), Family.Button);

			Expect(TEXT("stick neutral"), Press, Family.Neutral);
			Expect(TEXT("light push (aim zone)"), FString(TEXT("0:8a ")) + Press, Family.Neutral);
			Expect(TEXT("tilt"), FString(TEXT("0:8t ")) + Press, Family.Directional);
			Expect(TEXT("tilt (another direction)"), FString(TEXT("0:3t ")) + Press, Family.Directional);
			Expect(TEXT("crouch held, neutral"), FString(TEXT("0:+D ")) + Press, Family.CrouchNeutral);
			Expect(TEXT("crouch held, tilt"), FString(TEXT("0:+D 0:8t ")) + Press, Family.CrouchDirectional);
			Expect(TEXT("crouch pressed 2 frames after the attack"), Press + TEXT(" 7:+D"), Family.CrouchNeutral);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsComboTest, "GSTNCombat.Core.StandardSlots.ChordCommands", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsComboTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();
	using namespace GSTNCommandTags;

	const auto Expect = [&](const TCHAR* Notation, const FNativeGameplayTag& Expected)
	{
		const FGSTN_InputHistory History = PlayTo(*this, Notation, *Settings);
		TestEqual(*FString::Printf(TEXT("chord [%s]"), Notation), Trigger(Slots, History, *Settings).ToString(), Expected.GetTag().ToString());
	};
	Expect(TEXT("5:+A 5:+C"), Combo_AttackSpecial_Neutral);
	Expect(TEXT("5:+A 6:+C"), Combo_AttackSpecial_Neutral);
	Expect(TEXT("5:+C 6:+A"), Combo_AttackSpecial_Neutral);
	Expect(TEXT("0:8t 5:+A 5:+C"), Combo_AttackSpecial_Directional);
	Expect(TEXT("0:+D 5:+A 5:+C"), Combo_AttackSpecial_CrouchNeutral);
	Expect(TEXT("0:+D 0:8t 5:+A 5:+C"), Combo_AttackSpecial_CrouchDirectional);

	// A chord with the buttons too far apart is two commands: the most recent press wins alone.
	Expect(TEXT("5:+A 9:+C"), Special_Neutral);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsValidTest, "GSTNCombat.Core.StandardSlots.NoConflicts", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsValidTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();
	TArray<const FGSTN_InputSlotSpec*> Pointers;
	for (const FGSTN_InputSlotSpec& Spec : Slots)
	{
		Pointers.Add(&Spec);
	}

	const TArray<FGSTN_SlotConflict> Conflicts = UGSTN_InputSlotValidation::FindConflicts(Pointers, *Settings);
	for (const FGSTN_SlotConflict& Conflict : Conflicts)
	{
		AddError(Conflict.Message);
	}
	TestEqual(TEXT("the validator finds nothing in the standard set"), Conflicts.Num(), 0);

	// Every command tag is unique and valid, and every slot has a structurally valid pattern.
	TSet<FGameplayTag> Commands;
	for (const FGSTN_InputSlotSpec& Spec : Slots)
	{
		TestTrue(TEXT("valid command tag"), Spec.Command.IsValid());
		TestTrue(TEXT("valid pattern"), Spec.Pattern.IsStructurallyValid());
		Commands.Add(Spec.Command);
	}
	TestEqual(TEXT("16 distinct commands"), Commands.Num(), 16);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
