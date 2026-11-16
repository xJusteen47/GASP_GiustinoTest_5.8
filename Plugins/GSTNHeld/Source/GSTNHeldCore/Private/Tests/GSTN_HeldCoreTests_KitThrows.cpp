// Held objects, pure core test of M11 K2 (GSTNHeld.Core.KitThrowBlockers): which grab kit throws a lifted enemy switches off.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldKitThrowBlockersTest, "GSTNHeld.Core.KitThrowBlockers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldKitThrowBlockersTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Lifting = GSTNHeldTags::Held_CarryingEnemy;
	const auto Blockers = [](const FGSTN_EnemyThrowTuning& Tuning, FGameplayTagContainer& All, FGameplayTagContainer& StickOnly)
	{
		All.Reset();
		StickOnly.Reset();
		FGSTN_EnemyLiftRules::KitThrowBlockers(Tuning, All, StickOnly);
	};
	FGSTN_EnemyThrowTuning Tuning;
	FGameplayTagContainer All;
	FGameplayTagContainer StickOnly;

	Blockers(Tuning, All, StickOnly);
	TestTrue(TEXT("default: only the stick-only throws, while lifting"), All.IsEmpty() && StickOnly.HasTagExact(Lifting));
	Tuning.KitThrowsWhileLifting = EGSTN_LiftedEnemyKitThrows::None;
	Blockers(Tuning, All, StickOnly);
	TestTrue(TEXT("None: every kit throw"), All.HasTagExact(Lifting) && StickOnly.IsEmpty());
	Tuning.KitThrowsWhileLifting = EGSTN_LiftedEnemyKitThrows::All;
	Blockers(Tuning, All, StickOnly);
	TestTrue(TEXT("All: none blocked"), All.IsEmpty() && StickOnly.IsEmpty());
	Tuning.KitThrowsWhileLifting = EGSTN_LiftedEnemyKitThrows::None;
	Tuning.bEnabled = false;
	Blockers(Tuning, All, StickOnly);
	TestTrue(TEXT("thrown enemies off: none blocked"), All.IsEmpty() && StickOnly.IsEmpty());

	TestEqual(TEXT("project: the stick-only throws (DefaultGSTNHeld.ini)"), UGSTN_HeldRules::Get().ThrownEnemies.KitThrowsWhileLifting,
		EGSTN_LiftedEnemyKitThrows::NotStickOnly);
	return true;
}

#endif
