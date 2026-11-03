#include "Tests/GSTN_TestUtils.h"
#include "Tests/GSTN_TestBlueprintRules.h"
#include "Input/GSTN_BlueprintRules.h"

// The three native stand-ins sit outside the test guard because UHT reflects them whether or not tests are compiled.
bool UGSTN_TestCondition_StrongHeld::Matches_Implementation(const FGSTN_BlueprintConditionView& View) const
{
	return View.HeldButtons.HasTagExact(GSTNTestTags::Button_Strong.GetTag());
}

int32 UGSTN_TestCriterion_TierBonus::Score_Implementation(const FGSTN_BlueprintPriorityView& View) const
{
	return View.Tier >= 300 ? 50 : View.ManualOffset;
}

bool UGSTN_TestResetRule_AnyEventOrIdle::ShouldFlush_Implementation(const FGSTN_BlueprintResetView& View) const
{
	return !View.EventTags.IsEmpty() || View.FramesSinceLastInput >= 4;
}

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BlueprintConditionTest, "GSTNInput.Core.Input.Blueprint.Condition", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BlueprintConditionTest::RunTest(const FString& Parameters)
{
	FGSTN_InputCondition_Blueprint Wrapper;
	Wrapper.Condition = UGSTN_TestCondition_StrongHeld::StaticClass();
	Wrapper.Specificity = 7;
	Wrapper.ReferencedButtons.AddTag(Strong());

	const FGSTN_InputPattern WithStrong = Pattern({ Press(Attack()), MakeCondition(Wrapper) });
	TestEqual(TEXT("the wrapper weighs its specificity into the pattern"), WithStrong.GetSpecificity(), Pattern({ Press(Attack()) }).GetSpecificity() + 7);

	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory WithBoth = PlayTo(*this, TEXT("3:+B 5:+A"), *Settings);
	const FGSTN_InputHistory OnlyAttack = PlayTo(*this, TEXT("5:+A"), *Settings);
	TestTrue(TEXT("the Blueprint class decides: Strong is down"), MatchNow(WithStrong, WithBoth, *Settings));
	TestFalse(TEXT("the Blueprint class decides: Strong is not down"), MatchNow(WithStrong, OnlyAttack, *Settings));

	// Same input, same answer, however often it is asked: no state survives between calls
	TestTrue(TEXT("asking again gives the same answer"), MatchNow(WithStrong, WithBoth, *Settings));
	TestFalse(TEXT("asking the other history again gives the same answer"), MatchNow(WithStrong, OnlyAttack, *Settings));

	FGameplayTagContainer Referenced;
	Wrapper.GetReferencedButtons(Referenced);
	TestTrue(TEXT("the declared buttons are reported for the validation"), Referenced.HasTagExact(Strong()));

	// No class: the condition never holds instead of crashing
	FGSTN_InputCondition_Blueprint Empty;
	TestFalse(TEXT("a wrapper without a class does not match"), MatchNow(Pattern({ Press(Attack()), MakeCondition(Empty) }), WithBoth, *Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BlueprintPriorityTest, "GSTNInput.Core.Input.Blueprint.PriorityCriterion", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BlueprintPriorityTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_PriorityCriterion_Blueprint Wrapper;
	Wrapper.Criterion = UGSTN_TestCriterion_TierBonus::StaticClass();
	Settings->PriorityCriteria.Reset();
	Settings->PriorityCriteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_Blueprint>(Wrapper));

	FGSTN_InputSlotSpec SpecialSlot = Slot(GSTNTestTags::Special_Neutral, Pattern({ Press(GSTNTest::Special()) }));
	FGSTN_InputSlotSpec AttackSlot = Slot(GSTNTestTags::Attack_Neutral, Pattern({ Press(GSTNTest::Attack()) }));
	AttackSlot.PriorityOffset = 9;

	const FGSTN_CommandPriority SpecialPriority = SpecialSlot.GetPriority(*Settings);
	const FGSTN_CommandPriority AttackPriority = AttackSlot.GetPriority(*Settings);
	TestEqual(TEXT("one score"), SpecialPriority.Scores.Num(), 1);
	if (SpecialPriority.Scores.Num() == 1 && AttackPriority.Scores.Num() == 1)
	{
		TestEqual(TEXT("the special tier gets the bonus"), SpecialPriority.Scores[0], 50);
		TestEqual(TEXT("the attack tier gets its offset"), AttackPriority.Scores[0], 9);
	}

	// The Blueprint criterion takes part in the resolver like a C++ one
	const TArray<FGSTN_InputSlotSpec> Slots = { AttackSlot, SpecialSlot };
	TestEqual(TEXT("the special wins by the Blueprint score"), ResolveSlots(Slots, PlayTo(*this, TEXT("5:+A 5:+C"), *Settings), *Settings).SlotIndex, 1);

	FGSTN_PriorityCriterion_Blueprint Empty;
	Settings->PriorityCriteria.Reset();
	Settings->PriorityCriteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_Blueprint>(Empty));
	TestEqual(TEXT("a wrapper without a class scores 0"), SpecialSlot.GetPriority(*Settings).Scores[0], 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BlueprintResetRuleTest, "GSTNInput.Core.Input.Blueprint.ResetRule", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BlueprintResetRuleTest::RunTest(const FString& Parameters)
{
	using FRule = TInstancedStruct<FGSTN_BufferResetRule>;

	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_ButtonMap Map = GSTNTest::MakeButtonMap();

	FGSTN_BufferResetRule_Blueprint Wrapper;
	Wrapper.Rule = UGSTN_TestResetRule_AnyEventOrIdle::StaticClass();
	Wrapper.Buttons.AddTag(Attack());
	Wrapper.WatchedTags.AddTag(GSTNInputTags::Context_Airborne.GetTag());
	const FRule Rules[] = { FRule::Make<FGSTN_BufferResetRule_Blueprint>(Wrapper) };

	FGameplayTagContainer Watched;
	FGSTN_BufferResetRules::GetWatchedTags(Rules, Watched);
	TestTrue(TEXT("the declared tags are watched"), Watched.HasTagExact(GSTNInputTags::Context_Airborne.GetTag()));

	FGSTN_BufferFrameEvents Quiet;
	FGSTN_BufferFrameEvents Fired;
	Fired.EventTags.AddTag(GSTNInputTags::Context_Airborne.GetTag());

	{
		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 5:+B"), *Settings);
		TestFalse(TEXT("nothing happened, the input is fresh"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Quiet, 5));
		TestTrue(TEXT("an event makes the Blueprint flush"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Fired, 5));
		TestEqual(TEXT("only the rule's button is flushed"), History.GetHeldButtons(), 3u);
		TestTrue(TEXT("B is still a live press"), MatchNow(Pattern({ Press(Strong()) }), History, *Settings));
		TestFalse(TEXT("A is flushed"), MatchNow(Pattern({ Press(Attack()) }), History, *Settings));
	}
	{
		// The idle count is a function of the history
		FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings, 8);
		TestTrue(TEXT("idle for 3 frames: no"), !FGSTN_BufferResetRules::Apply(History, Map, Rules, Quiet, 8));
		TestTrue(TEXT("idle for 4 frames: yes"), FGSTN_BufferResetRules::Apply(History, Map, Rules, Quiet, 9));
	}

	const FRule Empty[] = { FRule::Make<FGSTN_BufferResetRule_Blueprint>() };
	FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings);
	TestFalse(TEXT("a wrapper without a class never flushes"), FGSTN_BufferResetRules::Apply(History, Map, Empty, Fired, 5));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
