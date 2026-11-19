// Automation tests of FGSTN_InputQueries (CP1 of the Blueprint seams): the input questions a move's Blueprint asks by button tag, answered
// from the rolled-back history. Pure: no world, no simulation.

#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputQueries.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNInputQueriesTest
{
	constexpr int32 ButtonA = 0;
	constexpr int32 ButtonB = 1;

	FGSTN_InputSample Sample(int32 Frame, uint32 Held, uint32 Pressed = 0, uint32 Released = 0)
	{
		FGSTN_InputSample Out;
		Out.Frame = Frame;
		Out.Held = Held;
		Out.Pressed = Pressed;
		Out.Released = Released;
		return Out;
	}

	/** Frames [From, To] appended: a tap of B on each frame in Taps, nothing else */
	void AppendTaps(FGSTN_InputHistory& History, int32 From, int32 To, const TArray<int32>& Taps)
	{
		for (int32 Frame = From; Frame <= To; ++Frame)
		{
			const uint32 Mask = Taps.Contains(Frame) ? (1u << ButtonB) : 0u;
			History.Append(Sample(Frame, 0u, Mask, Mask));
		}
	}

	FGSTN_InputQueries::FAgeContext At(int32 Now, int32 PausedStart = INDEX_NONE, int32 PausedEnd = INDEX_NONE)
	{
		FGSTN_InputQueries::FAgeContext Context;
		Context.Now = Now;
		Context.PausedStart = PausedStart;
		Context.PausedEnd = PausedEnd;
		return Context;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputQueriesPressAgeTest, "GSTNInput.Core.Input.Queries.PressAge", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputQueriesPressAgeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNInputQueriesTest;
	FGSTN_InputHistory History;
	AppendTaps(History, 0, 20, { 4, 15 });

	TestEqual(TEXT("the newest press of B is 5 frames old"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(20), true), 5);
	TestEqual(TEXT("A was never pressed"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonA, At(20), true), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("pressed within 5"), FGSTN_InputQueries::WasPressedWithin(History, ButtonB, At(20), 5, true));
	TestFalse(TEXT("not within 4"), FGSTN_InputQueries::WasPressedWithin(History, ButtonB, At(20), 4, true));
	TestFalse(TEXT("a negative window holds nothing"), FGSTN_InputQueries::WasPressedWithin(History, ButtonB, At(20), -1, true));
	TestFalse(TEXT("a button past the mask is never pressed"), FGSTN_InputQueries::WasPressedWithin(History, 40, At(20), 30, true));

	// Asked about an earlier frame (a question of the frame being simulated, never of the future): the press of 15 does not exist yet
	TestEqual(TEXT("at frame 10 the newest press is the one of frame 4"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(10), true), 6);
	TestEqual(TEXT("a press on this very frame is age 0"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(15), true), 0);

	// A press a command used: the move that started on it still sees it with bIncludeUsed, the buffer question does not
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const int32 Slot = History.GetButtonSlot(Age);
		const FGSTN_ButtonEvent Event = History.GetButtonEvent(Slot);
		if (Event.Frame == 15 && !Event.bRelease)
		{
			History.Consume(Slot);
		}
	}
	TestEqual(TEXT("used presses count when asked"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(20), true), 5);
	TestEqual(TEXT("unused only: the older press answers"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(20), false), 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputQueriesCountTest, "GSTNInput.Core.Input.Queries.DoublePressCount", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputQueriesCountTest::RunTest(const FString& Parameters)
{
	using namespace GSTNInputQueriesTest;
	// The double press of the projectile special: two taps 6 frames apart
	FGSTN_InputHistory History;
	AppendTaps(History, 0, 12, { 3, 9 });
	TestEqual(TEXT("two presses in the last 10 frames"), FGSTN_InputQueries::CountPressesWithin(History, ButtonB, At(12), 10, true), 2);
	TestEqual(TEXT("one press in the last 5"), FGSTN_InputQueries::CountPressesWithin(History, ButtonB, At(12), 5, true), 1);
	TestEqual(TEXT("no press of A"), FGSTN_InputQueries::CountPressesWithin(History, ButtonA, At(12), 10, true), 0);
	TestEqual(TEXT("releases are not presses"), FGSTN_InputQueries::CountPressesWithin(History, ButtonB, At(12), 100, true), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputQueriesHitstopTest, "GSTNInput.Core.Input.Queries.HitstopDoesNotAge", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputQueriesHitstopTest::RunTest(const FString& Parameters)
{
	using namespace GSTNInputQueriesTest;
	// Pressed on 10, hitstop [12, 20): at 22 the press is 12 frames old in simulation frames but 4 in combat frames, like the buffer counts it
	FGSTN_InputHistory History;
	AppendTaps(History, 0, 22, { 10 });
	TestEqual(TEXT("raw age without a window"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(22), true), 12);
	TestEqual(TEXT("hitstop frames do not count"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(22, 12, 20), true), 4);
	TestTrue(TEXT("still inside a 5 frame window after the hitstop"), FGSTN_InputQueries::WasPressedWithin(History, ButtonB, At(22, 12, 20), 5, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputQueriesHeldTest, "GSTNInput.Core.Input.Queries.HeldFrames", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputQueriesHeldTest::RunTest(const FString& Parameters)
{
	using namespace GSTNInputQueriesTest;
	const uint32 A = 1u << ButtonA;
	FGSTN_InputHistory History;
	History.Append(Sample(0, 0u));
	History.Append(Sample(1, A, A));
	for (int32 Frame = 2; Frame <= 8; ++Frame)
	{
		History.Append(Sample(Frame, A));
	}
	TestEqual(TEXT("held 7 frames at frame 8"), FGSTN_InputQueries::GetHeldFrames(History, ButtonA, 8), 7);
	TestEqual(TEXT("0 on the press frame"), FGSTN_InputQueries::GetHeldFrames(History, ButtonA, 1), 0);
	TestEqual(TEXT("B is up"), FGSTN_InputQueries::GetHeldFrames(History, ButtonB, 8), static_cast<int32>(INDEX_NONE));
	History.Append(Sample(9, 0u, 0u, A));
	TestEqual(TEXT("up after the release"), FGSTN_InputQueries::GetHeldFrames(History, ButtonA, 9), static_cast<int32>(INDEX_NONE));

	FGSTN_InputHistory Tap;
	AppendTaps(Tap, 0, 3, { 2 });
	TestEqual(TEXT("a tap is never down"), FGSTN_InputQueries::GetHeldFrames(Tap, ButtonB, 2), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputQueriesRollbackTest, "GSTNInput.Core.Input.Queries.Rollback", GSTN_CORE_TEST_FLAGS)
bool FGSTN_InputQueriesRollbackTest::RunTest(const FString& Parameters)
{
	using namespace GSTNInputQueriesTest;
	// The answers are a function of the history alone: a correction restores the history of frame 6 and the frames after it are simulated
	// again with another input (the server saw the second press on 9, not 8). Each frame's answer is the corrected one
	FGSTN_InputHistory History;
	AppendTaps(History, 0, 6, { 2 });
	const FGSTN_InputHistory Snapshot = History;
	AppendTaps(History, 7, 12, { 8 });
	TestEqual(TEXT("predicted: the second press is 4 frames old"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(12), true), 4);

	History = Snapshot;
	AppendTaps(History, 7, 12, { 9 });
	TestEqual(TEXT("corrected: 3 frames old"), FGSTN_InputQueries::GetNewestPressAge(History, ButtonB, At(12), true), 3);
	TestEqual(TEXT("still a double press"), FGSTN_InputQueries::CountPressesWithin(History, ButtonB, At(12), 10, true), 2);

	FGSTN_InputHistory Replay = Snapshot;
	AppendTaps(Replay, 7, 12, { 9 });
	TestTrue(TEXT("the same input gives the same history"), Replay == History);
	return true;
}

#endif
