// Held objects, pure core tests of slice 10 (GSTNHeld.Core.*): the throw's wind-up.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldThrowWindupTest, "GSTNHeld.Core.ThrowWindup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldThrowWindupTest::RunTest(const FString& Parameters)
{
	// The shipped defaults keep the throw instant (the throw before slice 10): code defaults and the project's ini
	TestTrue(TEXT("code default: instant"), FGSTN_HeldThrowWindup().IsInstant());
	TestTrue(TEXT("object default: no override"), !FGSTN_HeldObjectTuning().bOverrideWindup);
	TestTrue(TEXT("enemy default: instant"), FGSTN_EnemyThrowTuning().Windup.IsInstant());

	FGSTN_HeldThrowWindup Project;
	Project.ReleaseFrames = 20;
	FGSTN_HeldObjectTuning Object;
	Object.Windup.ReleaseFrames = 40;
	FGSTN_EnemyThrowTuning Enemy;
	Enemy.Windup.ReleaseFrames = 30;

	TestEqual(TEXT("an object without override: the project's"), FGSTN_HeldRules::ResolveWindup(&Object, nullptr, Project)->ReleaseFrames, 20);
	Object.bOverrideWindup = true;
	TestEqual(TEXT("an object that overrides: its own"), FGSTN_HeldRules::ResolveWindup(&Object, nullptr, Project)->ReleaseFrames, 40);
	TestEqual(TEXT("an object wins over a lifted enemy"), FGSTN_HeldRules::ResolveWindup(&Object, &Enemy, Project)->ReleaseFrames, 40);
	TestEqual(TEXT("a lifted enemy: the thrown-enemy tuning"), FGSTN_HeldRules::ResolveWindup(nullptr, &Enemy, Project)->ReleaseFrames, 30);
	TestNull(TEXT("nothing held: none"), FGSTN_HeldRules::ResolveWindup(nullptr, nullptr, Project));

	// Released exactly ReleaseFrames combat frames after the start; 0 = in the start frame itself
	TestFalse(TEXT("one frame early"), FGSTN_HeldRules::IsWindupReleased(100, 119, 20));
	TestTrue(TEXT("on the frame"), FGSTN_HeldRules::IsWindupReleased(100, 120, 20));
	TestTrue(TEXT("instant releases at once"), FGSTN_HeldRules::IsWindupReleased(100, 100, 0));
	return true;
}

#endif
