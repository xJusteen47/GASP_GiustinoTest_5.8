// UGSTN_FallingMode (owner 2026-11-25, dropkick bounce too early): while an attack hitbox is active, an exact zero vertical start speed (after a
// hitstop freeze, at a held apex) is handed to Mover's falling mode as a tiny downward speed, so its start-of-fall floor snap (up to 40 cm, a
// landing with no contact) does not run. The pure rules here; the in-game check is a capture on the desktop (handover dropkick-bounce-window).

#include "Misc/AutomationTest.h"
#include "Movement/GSTN_FallingMode.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FallingModeHitboxGateTest, "GSTNCombat.NP.FallingMode.HitboxGate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_FallingModeHitboxGateTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 60;
	FGSTN_TimelineTrack_Hitbox Kick;
	Kick.StartFrame = 15;
	Kick.EndFrame = 28;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Kick));
	FGSTN_TimelineTrack_Hurtbox Hurt;
	Hurt.StartFrame = 0;
	Hurt.EndFrame = 60;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hurtbox>(Hurt));

	TestFalse(TEXT("no timeline"), UGSTN_FallingMode::IsAttackHitboxActive(nullptr, 20));
	TestFalse(TEXT("no frame"), UGSTN_FallingMode::IsAttackHitboxActive(&Data, INDEX_NONE));
	TestFalse(TEXT("startup: only a hurtbox track holds it"), UGSTN_FallingMode::IsAttackHitboxActive(&Data, 5));
	TestTrue(TEXT("first active frame"), UGSTN_FallingMode::IsAttackHitboxActive(&Data, 15));
	TestTrue(TEXT("last active frame"), UGSTN_FallingMode::IsAttackHitboxActive(&Data, 27));
	TestFalse(TEXT("end is exclusive"), UGSTN_FallingMode::IsAttackHitboxActive(&Data, 28));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FallingModeNudgeTest, "GSTNCombat.NP.FallingMode.StartVelocityNudge",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_FallingModeNudgeTest::RunTest(const FString& Parameters)
{
	const FVector Up(0.0, 0.0, 1.0);
	const FVector Frozen(120.0, 0.0, 0.0);
	const FVector Nudged = UGSTN_FallingMode::NudgeStartVelocity(Frozen, Up, 0.01f);
	TestEqual(TEXT("horizontal kept"), Nudged.X, 120.0);
	TestTrue(TEXT("downward, past the parent's nearly-zero test"), Nudged.Z < 0.0 && !FMath::IsNearlyZero(Nudged.Dot(Up)));
	TestTrue(TEXT("tiny"), Nudged.Z > -0.02);
	const FVector Falling(0.0, 0.0, -300.0);
	TestTrue(TEXT("already falling: unchanged"), UGSTN_FallingMode::NudgeStartVelocity(Falling, Up, 0.01f).Equals(Falling, 0.0));
	const FVector Rising(0.0, 0.0, 50.0);
	TestTrue(TEXT("rising: unchanged"), UGSTN_FallingMode::NudgeStartVelocity(Rising, Up, 0.01f).Equals(Rising, 0.0));
	// Another gravity: the component along that up is the one nudged
	const FVector SideUp(1.0, 0.0, 0.0);
	const FVector Side = UGSTN_FallingMode::NudgeStartVelocity(FVector(0.0, 0.0, 200.0), SideUp, 0.01f);
	TestTrue(TEXT("nudged along the local up"), Side.X < 0.0 && FMath::IsNearlyEqual(Side.Z, 200.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
