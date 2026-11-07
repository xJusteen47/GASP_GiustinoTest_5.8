// E16: the BakeCheck comparison (combat-m5-editor.md 10.3).
#include "DebugDraw/GSTN_CombatBakeCheck.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugBakeCheckCompareTest, "GSTNCombat.Debug.BakeCheck.ComparePlacesBakedSocketLikeTheAttacker",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugBakeCheckCompareTest::RunTest(const FString& Parameters)
{
	// A pawn at (100, 0, 0) facing +Y; the baked socket 50 cm in front of the root (root-relative +X)
	const FTransform Root(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 0.0));
	const FTransform Baked(FRotator::ZeroRotator, FVector(50.0, 0.0, 0.0));

	const FTransform LiveSame(FRotator(0.0, 90.0, 0.0), FVector(100.0, 50.0, 0.0));
	FGSTN_BakeCheckResult Result = GSTNCombatBakeCheck::Compare(Baked, Root, LiveSame);
	TestTrue(TEXT("baked socket is placed in front of the pawn"), Result.BakedWorld.GetLocation().Equals(FVector(100.0, 50.0, 0.0), 0.01));
	TestEqual(TEXT("no gap"), Result.DistanceCm, 0.f, 0.01f);
	TestEqual(TEXT("no angle"), Result.AngleDeg, 0.f, 0.01f);

	const FTransform LiveOff(FRotator(0.0, 0.0, 0.0), FVector(100.0, 50.0, 12.0));
	Result = GSTNCombatBakeCheck::Compare(Baked, Root, LiveOff);
	TestEqual(TEXT("vertical gap"), Result.DistanceCm, 12.f, 0.01f);
	TestEqual(TEXT("yaw gap"), Result.AngleDeg, 90.f, 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DebugBakeCheckVariableTest, "GSTNCombat.Debug.BakeCheck.ConsoleVariableOffByDefault",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DebugBakeCheckVariableTest::RunTest(const FString& Parameters)
{
	const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Combat.Debug.BakeCheck"));
	if (TestNotNull(TEXT("gstn.Combat.Debug.BakeCheck exists"), Variable))
	{
		TestEqual(TEXT("off by default (host cost, 10.2b)"), Variable->GetInt(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
