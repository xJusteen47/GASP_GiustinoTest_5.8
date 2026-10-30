// Automation tests of the defence plugin: the tech state machine (plan F4).
// Rules for all tests: window 20 frames before the contact, lockout 40, no late frames, tumble.

#include "Data/GSTN_DefenceRules.h"
#include "GSTN_DefenceTestUtils.h"
#include "Math/GSTN_DefenceMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_TechRules TechTestRules()
	{
		FGSTN_TechRules Rules;
		Rules.WindowFrames = 20;
		Rules.LockoutFrames = 40;
		Rules.LateFrames = 0;
		Rules.bTechInHitstunNotTumble = false;
		return Rules;
	}

	FGSTN_TechEvaluation TechTestEvaluate(std::initializer_list<int32> Presses, int32 Contact, int32 HitstopEnd, bool bTumble, const FGSTN_TechRules& Rules, const FGSTN_TechState& State = FGSTN_TechState())
	{
		const TArray<int32> PressArray(Presses);
		return UGSTN_DefenceMath::EvaluateTech(State, PressArray, Contact, HitstopEnd, bTumble, Rules);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechInWindowTest, "GSTNDefence.Core.Tech.PressInWindow", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechInWindowTest::RunTest(const FString& Parameters)
{
	const FGSTN_TechRules Rules = TechTestRules();

	// A press 10 frames before the contact at 110
	const FGSTN_TechEvaluation Hit = TechTestEvaluate({ 100 }, 110, 80, true, Rules);
	TestTrue(TEXT("The contact was evaluated"), Hit.bContactEvaluated);
	TestTrue(TEXT("A press 10 frames before the contact teches"), Hit.bSuccess);
	TestEqual(TEXT("The press that made it"), Hit.PressFrame, 100);
	TestEqual(TEXT("The contact is marked handled"), Hit.State.LastHandledContact, 110);
	TestEqual(TEXT("A successful tech clears the lockout"), Hit.State.TechLockoutEndFrame, 0);

	// The edge: exactly WindowFrames before the contact
	TestTrue(TEXT("20 frames before the contact still teches"), TechTestEvaluate({ 90 }, 110, 80, true, Rules).bSuccess);
	TestFalse(TEXT("21 frames before the contact does not"), TechTestEvaluate({ 89 }, 110, 80, true, Rules).bSuccess);

	// No press at all
	const FGSTN_TechEvaluation NoPress = TechTestEvaluate({}, 110, 80, true, Rules);
	TestTrue(TEXT("A contact with no press is evaluated"), NoPress.bContactEvaluated);
	TestFalse(TEXT("and fails"), NoPress.bSuccess);
	TestEqual(TEXT("with no press frame"), NoPress.PressFrame, static_cast<int32>(INDEX_NONE));

	// The same contact is not evaluated twice
	const FGSTN_TechEvaluation Again = UGSTN_DefenceMath::EvaluateTech(Hit.State, TArray<int32>({ 100 }), 110, 80, true, Rules);
	TestFalse(TEXT("A contact already handled is skipped"), Again.bContactEvaluated);

	// A newer contact is evaluated
	const FGSTN_TechEvaluation Next = UGSTN_DefenceMath::EvaluateTech(Hit.State, TArray<int32>({ 100 }), 300, 80, true, Rules);
	TestTrue(TEXT("A newer contact is evaluated"), Next.bContactEvaluated);
	TestFalse(TEXT("and the old press is too old for it"), Next.bSuccess);

	// No contact yet: only the presses are processed
	const FGSTN_TechEvaluation Waiting = TechTestEvaluate({ 100 }, INDEX_NONE, 80, true, Rules);
	TestFalse(TEXT("No contact, no evaluation"), Waiting.bContactEvaluated);
	TestEqual(TEXT("but the press is stamped"), Waiting.State.LastTechPressFrame, 100);
	TestEqual(TEXT("and starts the lockout"), Waiting.State.TechLockoutEndFrame, 140);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechEarlyPressTest, "GSTNDefence.Core.Tech.EarlyPressLockout", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechEarlyPressTest::RunTest(const FString& Parameters)
{
	const FGSTN_TechRules Rules = TechTestRules();

	// An early press at 80 (30 frames before the contact at 110) starts a lockout until 120; the good press at 100 is locked out
	const FGSTN_TechEvaluation Result = TechTestEvaluate({ 80, 100 }, 110, 0, true, Rules);
	TestFalse(TEXT("The early press burned the tech"), Result.bSuccess);
	TestEqual(TEXT("The stamped press is the early one"), Result.State.LastTechPressFrame, 80);
	TestEqual(TEXT("The lockout ends 40 frames after it"), Result.State.TechLockoutEndFrame, 120);

	// A press after the lockout ended works again
	const FGSTN_TechEvaluation Later = TechTestEvaluate({ 80, 121 }, 130, 0, true, Rules);
	TestTrue(TEXT("A press once the lockout is over teches"), Later.bSuccess);
	TestEqual(TEXT("with that press"), Later.PressFrame, 121);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechMashTest, "GSTNDefence.Core.Tech.MashDoesNotExtend", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechMashTest::RunTest(const FString& Parameters)
{
	const FGSTN_TechRules Rules = TechTestRules();

	// Mashing during the lockout is ignored and does not extend it: presses at 100, 105, 110, 115
	const FGSTN_TechEvaluation Mash = TechTestEvaluate({ 100, 105, 110, 115 }, INDEX_NONE, 0, true, Rules);
	TestEqual(TEXT("Only the first press counted"), Mash.State.LastTechPressFrame, 100);
	TestEqual(TEXT("and the lockout still ends 40 frames after it"), Mash.State.TechLockoutEndFrame, 140);

	FGSTN_TechState State;
	TestTrue(TEXT("The first press counts"), UGSTN_DefenceMath::RegisterTechPress(State, 100, 0, true, Rules));
	TestFalse(TEXT("A press inside the lockout is ignored"), UGSTN_DefenceMath::RegisterTechPress(State, 139, 0, true, Rules));
	TestEqual(TEXT("and leaves the lockout alone"), State.TechLockoutEndFrame, 140);
	TestTrue(TEXT("The press on the last lockout frame +1 counts"), UGSTN_DefenceMath::RegisterTechPress(State, 140, 0, true, Rules));
	TestEqual(TEXT("and starts a new lockout"), State.TechLockoutEndFrame, 180);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechLateTest, "GSTNDefence.Core.Tech.LatePress", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechLateTest::RunTest(const FString& Parameters)
{
	FGSTN_TechRules Rules = TechTestRules();

	// By default a press after the contact is too late
	TestFalse(TEXT("LateFrames 0: a press 2 frames after the contact fails"), TechTestEvaluate({ 112 }, 110, 0, true, Rules).bSuccess);

	Rules.LateFrames = 3;
	TestTrue(TEXT("LateFrames 3: a press 2 frames after teches"), TechTestEvaluate({ 112 }, 110, 0, true, Rules).bSuccess);
	TestTrue(TEXT("LateFrames 3: a press 3 frames after teches"), TechTestEvaluate({ 113 }, 110, 0, true, Rules).bSuccess);
	TestFalse(TEXT("LateFrames 3: a press 4 frames after does not"), TechTestEvaluate({ 114 }, 110, 0, true, Rules).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechHitstopTest, "GSTNDefence.Core.Tech.HitstopExcluded", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechHitstopTest::RunTest(const FString& Parameters)
{
	const FGSTN_TechRules Rules = TechTestRules();

	// Hitstop ends at frame 100: a press during it never counts, even inside the window, and starts no lockout
	const FGSTN_TechEvaluation During = TechTestEvaluate({ 95 }, 110, 100, true, Rules);
	TestFalse(TEXT("A press during hitstop does not tech"), During.bSuccess);
	TestEqual(TEXT("and is not stamped"), During.State.LastTechPressFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and starts no lockout"), During.State.TechLockoutEndFrame, 0);

	// A press during hitstop does not block a press just after it
	const FGSTN_TechEvaluation Both = TechTestEvaluate({ 95, 100 }, 110, 100, true, Rules);
	TestTrue(TEXT("The press on the first frame after hitstop teches"), Both.bSuccess);
	TestEqual(TEXT("with that press"), Both.PressFrame, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechContactSameFrameTest, "GSTNDefence.Core.Tech.ContactSameFrame", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechContactSameFrameTest::RunTest(const FString& Parameters)
{
	const FGSTN_TechRules Rules = TechTestRules();
	const FGSTN_TechEvaluation Result = TechTestEvaluate({ 110 }, 110, 0, true, Rules);
	TestTrue(TEXT("A press on the frame of the contact teches"), Result.bSuccess);
	TestEqual(TEXT("with that press"), Result.PressFrame, 110);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceTechNoHitstunTest, "GSTNDefence.Core.Tech.NoTechInHitstun", GSTNDefenceTest::Flags)
bool FGSTN_DefenceTechNoHitstunTest::RunTest(const FString& Parameters)
{
	FGSTN_TechRules Rules = TechTestRules();

	// Hitstun without tumble: presses are ignored and start no lockout (m6 spec 6, step 3)
	const FGSTN_TechEvaluation Ignored = TechTestEvaluate({ 100 }, 110, 0, false, Rules);
	TestFalse(TEXT("A press in plain hitstun does not tech"), Ignored.bSuccess);
	TestEqual(TEXT("and is not stamped"), Ignored.State.LastTechPressFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and starts no lockout"), Ignored.State.TechLockoutEndFrame, 0);

	FGSTN_TechState State;
	TestFalse(TEXT("RegisterTechPress refuses it"), UGSTN_DefenceMath::RegisterTechPress(State, 100, 0, false, Rules));

	// The rules may allow it
	Rules.bTechInHitstunNotTumble = true;
	TestTrue(TEXT("With bTechInHitstunNotTumble the same press teches"), TechTestEvaluate({ 100 }, 110, 0, false, Rules).bSuccess);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
