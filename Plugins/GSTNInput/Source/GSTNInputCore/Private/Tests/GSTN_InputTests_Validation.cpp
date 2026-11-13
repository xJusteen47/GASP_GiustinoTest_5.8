#include "Tests/GSTN_TestUtils.h"
#include "Algo/Count.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Misc/DataValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	TArray<FGSTN_SlotConflict> Validate(const TArray<FGSTN_InputSlotSpec>& Slots, const UGSTN_InputSettings& Settings)
	{
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		return UGSTN_InputSlotValidation::FindConflicts(Pointers, Settings);
	}

	int32 Count(const TArray<FGSTN_SlotConflict>& Conflicts, EGSTN_SlotConflictKind Kind)
	{
		return Algo::CountIf(Conflicts, [Kind](const FGSTN_SlotConflict& Conflict) { return Conflict.Kind == Kind; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidationAmbiguousTest, "GSTNInput.Core.Input.Validation.Ambiguous", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ValidationAmbiguousTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// Two identical patterns in the same tier: the same input triggers both at the same rank.
	TArray<FGSTN_InputSlotSpec> Slots = {
		Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })),
		Slot(GSTNTestTags::Attack_Directional, Pattern({ Press(Attack()) })),
	};
	TArray<FGSTN_SlotConflict> Conflicts = Validate(Slots, *Settings);
	TestEqual(TEXT("one ambiguity, reported once"), Count(Conflicts, EGSTN_SlotConflictKind::Ambiguous), 1);
	TestEqual(TEXT("and nothing else"), Conflicts.Num(), 1);
	if (!Conflicts.IsEmpty())
	{
		TestTrue(TEXT("it is an error"), Conflicts[0].IsError());
		TestTrue(TEXT("it names both commands"), Conflicts[0].Message.Contains(TEXT("Command.Test.Attack.Neutral")) && Conflicts[0].Message.Contains(TEXT("Command.Test.Attack.Directional")));
	}

	// The manual offset resolves it, but now the lower one is shadowed (a warning, not an error).
	Slots[1].PriorityOffset = 1;
	Conflicts = Validate(Slots, *Settings);
	TestEqual(TEXT("no more ambiguity"), Count(Conflicts, EGSTN_SlotConflictKind::Ambiguous), 0);
	TestEqual(TEXT("the first slot is shadowed"), Count(Conflicts, EGSTN_SlotConflictKind::Shadowed), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidationShadowedTest, "GSTNInput.Core.Input.Validation.Shadowed", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ValidationShadowedTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// A button alone and the same button with a direction: different ranks, neither is shadowed.
	{
		const TArray<FGSTN_InputSlotSpec> Slots = {
			Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })),
			Slot(GSTNTestTags::Attack_Directional, Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) })),
		};
		const TArray<FGSTN_SlotConflict> Conflicts = Validate(Slots, *Settings);
		TestEqual(TEXT("button vs direction + button: no finding"), Conflicts.Num(), 0);
	}

	// The same button as a special (higher tier): the attack can never fire.
	{
		const TArray<FGSTN_InputSlotSpec> Slots = {
			Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })),
			Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Attack()) })),
		};
		const TArray<FGSTN_SlotConflict> Conflicts = Validate(Slots, *Settings);
		TestEqual(TEXT("one finding"), Conflicts.Num(), 1);
		if (!Conflicts.IsEmpty())
		{
			TestEqual(TEXT("it is a shadow"), Conflicts[0].Kind, EGSTN_SlotConflictKind::Shadowed);
			TestEqual(TEXT("of the attack"), Conflicts[0].SlotA, 0);
			TestEqual(TEXT("by the special"), Conflicts[0].SlotB, 1);
			TestFalse(TEXT("only a warning"), Conflicts[0].IsError());
		}
	}

	// Tier before specificity: a plain special shadows nothing that needs the special button too.
	{
		const TArray<FGSTN_InputSlotSpec> Slots = {
			Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })),
			Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(Special()) })),
		};
		TestEqual(TEXT("different buttons"), Validate(Slots, *Settings).Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidationInvalidTest, "GSTNInput.Core.Input.Validation.InvalidSlots", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ValidationInvalidTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// The same command tag twice.
	{
		const TArray<FGSTN_InputSlotSpec> Slots = {
			Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Attack()) })),
			Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(Strong()) })),
		};
		const TArray<FGSTN_SlotConflict> Conflicts = Validate(Slots, *Settings);
		TestEqual(TEXT("duplicate command"), Count(Conflicts, EGSTN_SlotConflictKind::DuplicateCommand), 1);
		TestTrue(TEXT("an error"), Conflicts.Num() > 0 && Conflicts[0].IsError());
	}

	// No command tag, no steps, a step without anchor.
	{
		const TArray<FGSTN_InputSlotSpec> Slots = {
			Slot(FGameplayTag(), Pattern({ Press(Attack()) })),
			Slot(GSTNTestTags::Attack_Directional, FGSTN_InputPattern()),
			Slot(GSTNTestTags::Strong_Neutral, Pattern({ Hold(Crouch()) })),
		};
		const TArray<FGSTN_SlotConflict> Conflicts = Validate(Slots, *Settings);
		TestEqual(TEXT("three invalid slots"), Count(Conflicts, EGSTN_SlotConflictKind::InvalidSlot), 3);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
