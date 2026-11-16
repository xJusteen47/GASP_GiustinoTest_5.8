#include "Tests/GSTN_CombatTestUtils.h"
#include "Stats/GSTN_StatsScaling.h"
#include "Timeline/GSTN_WeaponSwitchRules.h"
#include "Tests/GSTN_StatsTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StatsFactorTest, "GSTNCombat.Core.Stats.Factor", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StatsFactorTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("the reference stat scales by 1"), FGSTN_StatsScaling::ComputeFactor(600.f, 600.f), 1.f);
	TestEqual(TEXT("twice the reference doubles"), FGSTN_StatsScaling::ComputeFactor(1200.f, 600.f), 2.f);
	TestEqual(TEXT("a stat of 0 stops"), FGSTN_StatsScaling::ComputeFactor(0.f, 600.f), 0.f);
	TestEqual(TEXT("a negative stat never reverses"), FGSTN_StatsScaling::ComputeFactor(-50.f, 600.f), 0.f);
	TestEqual(TEXT("no reference means no scaling"), FGSTN_StatsScaling::ComputeFactor(300.f, 0.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StatsScalingBaseRelativeTest, "GSTNCombat.Core.Stats.ScalingIsBaseRelative", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StatsScalingBaseRelativeTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_TestSpeedHolder> Base(NewObject<UGSTN_TestSpeedHolder>(GetTransientPackage()));
	Base->RunSpeed = 375.f;
	Base->SprintSpeed = 585.0;
	Base->Unlisted = 10.f;
	const TStrongObjectPtr<UGSTN_TestSpeedHolder> Target(DuplicateObject<UGSTN_TestSpeedHolder>(Base.Get(), GetTransientPackage()));

	const TArray<FName> Names = { TEXT("RunSpeed"), TEXT("SprintSpeed"), TEXT("NoSuchProperty"), TEXT("Label") };
	TestEqual(TEXT("the float and the double are written, the unknown name and the non-number are skipped"), FGSTN_StatsScaling::ApplyScaledFloats(*Target, *Base, Names, 2.f), 2);
	TestEqual(TEXT("float is base * 2"), Target->RunSpeed, 750.f);
	TestEqual(TEXT("double is base * 2"), Target->SprintSpeed, 1170.0);
	TestEqual(TEXT("a property that is not listed is left alone"), Target->Unlisted, 10.f);

	// The same factor again changes nothing: the base is the archetype, not the current value
	FGSTN_StatsScaling::ApplyScaledFloats(*Target, *Base, Names, 2.f);
	TestEqual(TEXT("writing the factor twice does not compound"), Target->RunSpeed, 750.f);

	// A different factor after another one, as after a rollback to a frame with another stat, lands on base * factor too
	FGSTN_StatsScaling::ApplyScaledFloats(*Target, *Base, Names, 0.5f);
	TestEqual(TEXT("a new factor is relative to the base"), Target->RunSpeed, 187.5f);
	FGSTN_StatsScaling::ApplyScaledFloats(*Target, *Base, Names, 1.f);
	TestEqual(TEXT("factor 1 restores the authored value"), Target->RunSpeed, 375.f);
	TestEqual(TEXT("for the double too"), Target->SprintSpeed, 585.0);

	FGSTN_StatsScaling::ApplyScaledFloats(*Target, *Base, Names, 0.f);
	TestEqual(TEXT("factor 0 stops the speed"), Target->RunSpeed, 0.f);

	double Read = 0.0;
	TestTrue(TEXT("ReadFloat reads a float"), FGSTN_StatsScaling::ReadFloat(*Base, TEXT("RunSpeed"), Read));
	TestEqual(TEXT("its value"), Read, 375.0);
	TestFalse(TEXT("ReadFloat refuses a non-number"), FGSTN_StatsScaling::ReadFloat(*Base, TEXT("Label"), Read));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WeaponSwitchResolveTest, "GSTNCombat.Core.Stats.WeaponSwitchResolve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_WeaponSwitchResolveTest::RunTest(const FString& Parameters)
{
	using R = FGSTN_WeaponSwitchRules;
	TestEqual(TEXT("next goes forward"), R::Resolve(EGSTN_SwitchTarget::Next, 0, 0, 3), 1);
	TestEqual(TEXT("next wraps"), R::Resolve(EGSTN_SwitchTarget::Next, 0, 2, 3), 0);
	TestEqual(TEXT("previous wraps"), R::Resolve(EGSTN_SwitchTarget::Previous, 0, 0, 3), 2);
	TestEqual(TEXT("previous goes back"), R::Resolve(EGSTN_SwitchTarget::Previous, 0, 2, 3), 1);
	TestEqual(TEXT("index selects"), R::Resolve(EGSTN_SwitchTarget::Index, 2, 0, 3), 2);
	TestEqual(TEXT("an index the loadout does not have changes nothing"), R::Resolve(EGSTN_SwitchTarget::Index, 5, 1, 3), 1);
	TestEqual(TEXT("one weapon never switches"), R::Resolve(EGSTN_SwitchTarget::Next, 0, 0, 1), 0);
	TestEqual(TEXT("no weapon never switches"), R::Resolve(EGSTN_SwitchTarget::Next, 0, 0, 0), 0);
	TestEqual(TEXT("an active index out of range counts as the first weapon"), R::Resolve(EGSTN_SwitchTarget::Next, 0, 9, 2), 1);
	return true;
}

#endif
