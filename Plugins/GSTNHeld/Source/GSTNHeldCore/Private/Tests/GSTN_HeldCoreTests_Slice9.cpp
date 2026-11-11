// Held objects, pure core tests of slice 9 (GSTNHeld.Core.*): the plan's G9 thrown enemies (lift rules, the enemy throw slot).

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldEnemyLiftTest, "GSTNHeld.Core.EnemyLift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldEnemyLiftTest::RunTest(const FString& Parameters)
{
	FGSTN_EnemyThrowTuning Tuning;
	Tuning.DefaultEnemyWeight = 40.f;
	Tuning.LiftRule = EGSTN_EnemyLiftRule::WeightVsCapacity;
	TestTrue(TEXT("weight within capacity lifts"), FGSTN_EnemyLiftRules::CanLift(Tuning, 45.f, 50.f, false));
	TestTrue(TEXT("weight equal to capacity lifts"), FGSTN_EnemyLiftRules::CanLift(Tuning, 50.f, 50.f, false));
	TestFalse(TEXT("heavier than the capacity does not"), FGSTN_EnemyLiftRules::CanLift(Tuning, 51.f, 50.f, false));
	TestTrue(TEXT("no weight attribute: the default weight"), FGSTN_EnemyLiftRules::CanLift(Tuning, 0.f, 50.f, false));
	TestFalse(TEXT("the default weight over a weak grabber"), FGSTN_EnemyLiftRules::CanLift(Tuning, 0.f, 30.f, false));
	TestFalse(TEXT("the no-lift tag forbids it"), FGSTN_EnemyLiftRules::CanLift(Tuning, 1.f, 50.f, true));

	Tuning.LiftRule = EGSTN_EnemyLiftRule::AnyGrabbed;
	TestTrue(TEXT("any grabbed: weight ignored"), FGSTN_EnemyLiftRules::CanLift(Tuning, 500.f, 50.f, false));
	TestFalse(TEXT("any grabbed: the tag still forbids"), FGSTN_EnemyLiftRules::CanLift(Tuning, 1.f, 50.f, true));

	Tuning.bEnabled = false;
	TestFalse(TEXT("off: never"), FGSTN_EnemyLiftRules::CanLift(Tuning, 1.f, 50.f, false));

	Tuning.FullLoadSpeedScale = 0.5f;
	TestEqual(TEXT("no load: full speed"), FGSTN_EnemyLiftRules::SpeedScale(Tuning, 0.f), 1.f);
	TestEqual(TEXT("full load: the setting"), FGSTN_EnemyLiftRules::SpeedScale(Tuning, 1.f), 0.5f);
	TestEqual(TEXT("over full load clamps"), FGSTN_EnemyLiftRules::SpeedScale(Tuning, 3.f), 0.5f);
	TestEqual(TEXT("the attribute wins over the default"), FGSTN_EnemyLiftRules::EnemyWeight(Tuning, 70.f), 70.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldEnemyThrowSlotTest, "GSTNHeld.Core.EnemyThrowSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldEnemyThrowSlotTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHeldTags;
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_HeldSlots::Build();
	const TArray<FGSTN_HeldSlotBinding> Bindings = UGSTN_HeldSlots::DefaultBindings();
	const FGSTN_InputSlotSpec* Enemy = Slots.FindByPredicate([](const FGSTN_InputSlotSpec& S) { return S.Command == Command_Held_ThrowEnemy.GetTag(); });
	const FGSTN_InputSlotSpec* Object = Slots.FindByPredicate([](const FGSTN_InputSlotSpec& S) { return S.Command == Command_Held_Throw.GetTag(); });
	if (!TestNotNull(TEXT("an enemy throw slot"), Enemy) || !TestNotNull(TEXT("an object throw slot"), Object))
	{
		return false;
	}
	TestTrue(TEXT("its own priority, never ambiguous with the object throw"), Enemy->PriorityOffset != Object->PriorityOffset);
	TestTrue(TEXT("above the grab's kit throws (link role offset 100)"), Enemy->PriorityOffset > 100);
	const FGSTN_HeldSlotBinding* Binding = Bindings.FindByPredicate([](const FGSTN_HeldSlotBinding& B) { return B.Command == Command_Held_ThrowEnemy.GetTag(); });
	TestTrue(TEXT("requires a lifted enemy, not an object"), Binding && Binding->Requirements.RequireTags.HasTagExact(Held_CarryingEnemy)
		&& Binding->Requirements.IgnoreTags.HasTagExact(Held_Carrying));
	TestTrue(TEXT("the thrown enemy source is a thrown source"), Source_ThrownEnemy.GetTag().MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Combat.Source.Thrown"))));
	return true;
}

#endif
