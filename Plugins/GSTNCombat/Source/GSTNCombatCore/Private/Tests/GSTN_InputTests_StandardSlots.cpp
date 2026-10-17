#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_StandardSlots.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	/** The command the standard set triggers for a history, empty if none */
	FGameplayTag Trigger(const TArray<FGSTN_InputSlotSpec>& Slots, const FGSTN_InputHistory& History, const UGSTN_CombatInputSettings& Settings, bool bAirborne = false)
	{
		FGameplayTagContainer Owner;
		if (bAirborne)
		{
			Owner.AddTag(GSTNCommandTags::Context_Airborne.GetTag());
		}
		const FGSTN_SlotResolution Resolution = ResolveSlots(Slots, History, Settings, Owner);
		return Resolution.SlotIndex == INDEX_NONE ? FGameplayTag() : Slots[Resolution.SlotIndex].Command;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsShapeTest, "GSTNCombat.Core.Input.StandardSlots.Shapes", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsShapeTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();
	TestEqual(TEXT("24 family commands and 4 chord commands"), Slots.Num(), 28);

	struct FFamily
	{
		const TCHAR* Button;
		const FNativeGameplayTag& Neutral;
		const FNativeGameplayTag& Directional;
		const FNativeGameplayTag& CrouchNeutral;
		const FNativeGameplayTag& CrouchDirectional;
		const FNativeGameplayTag& AirNeutral;
		const FNativeGameplayTag& AirDirectional;
		const FNativeGameplayTag& AirCrouchNeutral;
		const FNativeGameplayTag& AirCrouchDirectional;
	};
	using namespace GSTNCommandTags;
	const FFamily Families[] = {
		{ TEXT("A"), Attack_Neutral, Attack_Directional, Attack_CrouchNeutral, Attack_CrouchDirectional, Air_Attack_Neutral, Air_Attack_Directional, Air_Attack_CrouchNeutral, Air_Attack_CrouchDirectional },
		{ TEXT("B"), Strong_Neutral, Strong_Directional, Strong_CrouchNeutral, Strong_CrouchDirectional, Air_Strong_Neutral, Air_Strong_Directional, Air_Strong_CrouchNeutral, Air_Strong_CrouchDirectional },
		{ TEXT("C"), Special_Neutral, Special_Directional, Special_CrouchNeutral, Special_CrouchDirectional, Air_Special_Neutral, Air_Special_Directional, Air_Special_CrouchNeutral, Air_Special_CrouchDirectional },
	};

	// Camera and character turned or not: the results must not change.
	const TCHAR* Orientations[] = { TEXT(""), TEXT("0:cam=90 0:face=90 "), TEXT("0:cam=200 0:face=20 ") };

	for (const FFamily& Family : Families)
	{
		for (const TCHAR* Orientation : Orientations)
		{
			for (const bool bAirborne : { false, true })
			{
				const auto Expect = [&](const TCHAR* What, const FString& Notation, const FNativeGameplayTag& Ground, const FNativeGameplayTag& Air, int32 Now = INDEX_NONE)
				{
					const FGSTN_InputHistory History = PlayTo(*this, *(FString(Orientation) + Notation), *Settings, Now);
					const FGameplayTag Expected = (bAirborne ? Air : Ground).GetTag();
					TestEqual(*FString::Printf(TEXT("%s, button %s, %s, orientation \"%s\""), What, Family.Button, bAirborne ? TEXT("airborne") : TEXT("grounded"), Orientation),
						Trigger(Slots, History, *Settings, bAirborne).ToString(), Expected.ToString());
				};
				const FString Press = FString::Printf(TEXT("5:+%s"), Family.Button);

				Expect(TEXT("stick neutral"), Press, Family.Neutral, Family.AirNeutral);
				Expect(TEXT("light push (aim zone)"), FString(TEXT("0:8a ")) + Press, Family.Neutral, Family.AirNeutral);
				Expect(TEXT("tilt"), FString(TEXT("0:8t ")) + Press, Family.Directional, Family.AirDirectional);
				Expect(TEXT("tilt (another direction)"), FString(TEXT("0:3t ")) + Press, Family.Directional, Family.AirDirectional);
				Expect(TEXT("crouch held, neutral"), FString(TEXT("0:+D ")) + Press, Family.CrouchNeutral, Family.AirCrouchNeutral);
				Expect(TEXT("crouch held, tilt"), FString(TEXT("0:+D 0:8t ")) + Press, Family.CrouchDirectional, Family.AirCrouchDirectional);
				Expect(TEXT("crouch pressed 2 frames after the attack"), Press + TEXT(" 7:+D"), Family.CrouchNeutral, Family.AirCrouchNeutral);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsComboTest, "GSTNCombat.Core.Input.StandardSlots.ChordCommands", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsComboTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
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

	// Airborne: there is no airborne chord, the special wins (the higher tier of the two singles).
	{
		const FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 5:+C"), *Settings);
		TestEqual(TEXT("airborne attack + special"), Trigger(Slots, History, *Settings, true).ToString(), Air_Special_Neutral.GetTag().ToString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StandardSlotsValidTest, "GSTNCombat.Core.Input.StandardSlots.NoConflicts", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StandardSlotsValidTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CombatInputSettings> Settings = MakeSettings();
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
	TestEqual(TEXT("28 distinct commands"), Commands.Num(), 28);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
