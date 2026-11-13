#include "Tests/GSTN_TestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	/** Plays a notation up to Now and matches the pattern with the given params customisation */
	bool MatchAt(FAutomationTestBase& Test, const FGSTN_InputPattern& Pattern, const TCHAR* Notation, int32 Now,
		TFunctionRef<void(FGSTN_MatchParams&)> Customize = [](FGSTN_MatchParams&) {}, FGSTN_PatternMatch* OutMatch = nullptr)
	{
		const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
		const FGSTN_InputHistory History = PlayTo(Test, Notation, *Settings, Now);
		return MatchNow(Pattern, History, *Settings, OutMatch, Customize);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BufferTimeTest, "GSTNInput.Core.Input.Buffer.TimeWindow", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BufferTimeTest::RunTest(const FString& Parameters)
{
	const FGSTN_InputPattern PressA = Pattern({ Press(Attack()) });
	const auto NoHold = [](FGSTN_MatchParams& Params) { Params.bHoldBuffer = false; };

	// Tap at frame 2, time buffer 8: valid up to frame 10.
	TestTrue(TEXT("the frame of the press"), MatchAt(*this, PressA, TEXT("2:*A"), 2, NoHold));
	TestTrue(TEXT("press at Now - BufferFrames"), MatchAt(*this, PressA, TEXT("2:*A"), 10, NoHold));
	TestFalse(TEXT("press at Now - BufferFrames - 1"), MatchAt(*this, PressA, TEXT("2:*A"), 11, NoHold));

	// A custom window.
	TestTrue(TEXT("window of 3, press 3 frames ago"), MatchAt(*this, PressA, TEXT("2:*A"), 5, [](FGSTN_MatchParams& Params) { Params.bHoldBuffer = false; Params.BufferFrames = 3; }));
	TestFalse(TEXT("window of 3, press 4 frames ago"), MatchAt(*this, PressA, TEXT("2:*A"), 6, [](FGSTN_MatchParams& Params) { Params.bHoldBuffer = false; Params.BufferFrames = 3; }));

	// Paused frames (hitstop) do not age the press: one paused frame extends the window by one.
	const auto Paused = [](int32 Start, int32 End)
	{
		return [Start, End](FGSTN_MatchParams& Params) { Params.bHoldBuffer = false; Params.PausedStart = Start; Params.PausedEnd = End; };
	};
	TestTrue(TEXT("one paused frame extends the window"), MatchAt(*this, PressA, TEXT("2:*A"), 11, Paused(5, 6)));
	TestFalse(TEXT("two frames over the window with one paused"), MatchAt(*this, PressA, TEXT("2:*A"), 12, Paused(5, 6)));
	TestTrue(TEXT("a longer pause"), MatchAt(*this, PressA, TEXT("2:*A"), 14, Paused(4, 8)));
	TestFalse(TEXT("a longer pause, one frame later"), MatchAt(*this, PressA, TEXT("2:*A"), 15, Paused(4, 8)));
	TestFalse(TEXT("pause before the press does not count"), MatchAt(*this, PressA, TEXT("2:*A"), 11, Paused(0, 2)));
	TestFalse(TEXT("pause after now does not count"), MatchAt(*this, PressA, TEXT("2:*A"), 11, Paused(11, 20)));

	// Age helper.
	FGSTN_MatchParams Params;
	Params.Now = 20;
	TestEqual(TEXT("age without pause"), FGSTN_InputMatcher::GetAge(12, Params), 8);
	Params.PausedStart = 14;
	Params.PausedEnd = 17;
	TestEqual(TEXT("age with a pause inside"), FGSTN_InputMatcher::GetAge(12, Params), 5);
	TestEqual(TEXT("age of an event inside the pause"), FGSTN_InputMatcher::GetAge(15, Params), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BufferHoldTest, "GSTNInput.Core.Input.Buffer.HoldBuffer", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BufferHoldTest::RunTest(const FString& Parameters)
{
	const FGSTN_InputPattern PressA = Pattern({ Press(Attack()) });
	FGSTN_PatternMatch Match;

	// A press held since frame 0 stays valid past the window.
	TestTrue(TEXT("held press past the window"), MatchAt(*this, PressA, TEXT("0:+A"), 30, [](FGSTN_MatchParams&) {}, &Match));
	TestTrue(TEXT("it came through the hold buffer"), Match.bViaHoldBuffer);
	TestFalse(TEXT("a press inside the window is not via the hold buffer"), MatchAt(*this, PressA, TEXT("0:+A"), 5, [](FGSTN_MatchParams&) {}, &Match) && Match.bViaHoldBuffer);

	// Released: only the time window applies.
	TestTrue(TEXT("released inside the window"), MatchAt(*this, PressA, TEXT("0:+A 4:-A"), 8, [](FGSTN_MatchParams&) {}));
	TestFalse(TEXT("released and past the window"), MatchAt(*this, PressA, TEXT("0:+A 4:-A"), 30, [](FGSTN_MatchParams&) {}));

	// Hold buffer off.
	TestFalse(TEXT("hold buffer off"), MatchAt(*this, PressA, TEXT("0:+A"), 30, [](FGSTN_MatchParams& Params) { Params.bHoldBuffer = false; }));

	// A limit on how long a held press waits (age 20 is the last valid frame).
	const auto LimitedTo20 = [](FGSTN_MatchParams& Params) { Params.MaxHoldBufferFrames = 20; };
	TestTrue(TEXT("held press at the limit"), MatchAt(*this, PressA, TEXT("0:+A"), 20, LimitedTo20));
	TestFalse(TEXT("held press past the limit"), MatchAt(*this, PressA, TEXT("0:+A"), 21, LimitedTo20));

	// Another press of the same button replaces the old one: the old press was released in between.
	TestTrue(TEXT("press, release, press again and hold"), MatchAt(*this, PressA, TEXT("0:+A 3:-A 20:+A"), 40));

	// Consuming a press ends its hold buffering.
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputHistory History = PlayTo(*this, TEXT("0:+A"), *Settings, 30);
	if (!TestTrue(TEXT("held press matches"), MatchNow(PressA, History, *Settings, &Match)))
	{
		return true;
	}
	History.Consume(Match.UsedButtonEvents[0]);
	TestFalse(TEXT("consumed press does not match"), MatchNow(PressA, History, *Settings));

	// Flushing a button (reset rules) too, and the next press works again.
	FGSTN_InputHistory Flushed = PlayTo(*this, TEXT("0:+A"), *Settings, 30);
	Flushed.FlushPresses(1u, 30);
	TestFalse(TEXT("flushed press does not match"), MatchNow(PressA, Flushed, *Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BufferDirectionPolicyTest, "GSTNInput.Core.Input.Buffer.DirectionPolicy", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BufferDirectionPolicyTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const FGSTN_InputPattern AttackRight = Pattern({ Step({ Press(Attack()),
		Direction(EGSTN_DirectionSpace::Facing, FromDirection(EDir::Right), EGSTN_StickZone::Tilt) }) });
	const auto AtPress = [](FGSTN_MatchParams& Params) { Params.DirectionPolicy = EGSTN_DirectionPolicy::AtPress; };
	const auto AtExecution = [](FGSTN_MatchParams& Params) { Params.DirectionPolicy = EGSTN_DirectionPolicy::AtExecution; };

	// Right at the press, left when the command would come out (frame 9).
	const TCHAR* RightThenLeft = TEXT("0:6t 3:+A 5:4t");
	TestTrue(TEXT("AtPress: the stick was right when pressed"), MatchAt(*this, AttackRight, RightThenLeft, 9, AtPress));
	TestFalse(TEXT("AtExecution: the stick is left now"), MatchAt(*this, AttackRight, RightThenLeft, 9, AtExecution));

	// Neutral at the press, right when the command would come out.
	const TCHAR* NeutralThenRight = TEXT("3:+A 8:6t");
	TestFalse(TEXT("AtPress: the stick was neutral when pressed"), MatchAt(*this, AttackRight, NeutralThenRight, 9, AtPress));
	TestTrue(TEXT("AtExecution: the stick is right now"), MatchAt(*this, AttackRight, NeutralThenRight, 9, AtExecution));

	// The direction policy does not touch earlier steps.
	const FGSTN_InputPattern RightThenAttack = Pattern({ Step({ Press(Strong()), Direction(EGSTN_DirectionSpace::Facing, FromDirection(EDir::Right), EGSTN_StickZone::Tilt) }), Step({ Press(Attack()) }) });
	TestTrue(TEXT("earlier steps are judged at their own anchor"), MatchAt(*this, RightThenAttack, TEXT("0:6t 2:+B 3:5 5:+A"), 7, AtExecution));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BufferStepsTest, "GSTNInput.Core.Input.Buffer.Steps", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BufferStepsTest::RunTest(const FString& Parameters)
{
	const auto Sequence = [](int32 Gap) { return Pattern({ Step({ Press(Attack()) }, -1, Gap), Step({ Press(Strong()) }) }); };

	TestTrue(TEXT("two steps in order"), MatchAt(*this, Sequence(5), TEXT("0:*A 3:*B"), 3));
	TestFalse(TEXT("two steps in the wrong order"), MatchAt(*this, Sequence(5), TEXT("0:*B 3:*A"), 3));
	TestTrue(TEXT("gap at the limit"), MatchAt(*this, Sequence(5), TEXT("0:*A 5:*B"), 5));
	TestFalse(TEXT("gap over the limit"), MatchAt(*this, Sequence(5), TEXT("0:*A 6:*B"), 6));

	// Both steps may happen on the same frame.
	TestTrue(TEXT("same frame"), MatchAt(*this, Sequence(5), TEXT("4:+A 4:+B"), 4));

	// An event used by a later step is not available to an earlier one: a double tap needs two presses.
	const FGSTN_InputPattern DoubleTap = Pattern({ Step({ Press(Attack()) }, -1, 10), Step({ Press(Attack()) }) });
	TestFalse(TEXT("one press is not a double tap"), MatchAt(*this, DoubleTap, TEXT("3:*A"), 3));
	TestTrue(TEXT("two presses are"), MatchAt(*this, DoubleTap, TEXT("3:*A 6:*A"), 6));
	TestFalse(TEXT("two presses too far apart"), MatchAt(*this, DoubleTap, TEXT("0:*A 11:*A"), 11));

	// Backtracking: the newest press of the first step lacks the modifier, the older one has it.
	const FGSTN_InputPattern WithModifier = Pattern({ Step({ Press(Attack()), Hold(Crouch()) }, -1, 8), Step({ Press(Strong()) }) });
	FGSTN_PatternMatch Match;
	if (!TestTrue(TEXT("falls back to the older press"), MatchAt(*this, WithModifier, TEXT("0:+D 1:*A 2:-D 7:*A 8:*B"), 8, [](FGSTN_MatchParams&) {}, &Match)))
	{
		return true;
	}
	TestEqual(TEXT("it took the older press"), Match.StepAnchors[0], 1);
	TestEqual(TEXT("and the terminal one"), Match.StepAnchors[1], 8);
	TestEqual(TEXT("two events used"), Match.UsedButtonEvents.Num(), 2);

	// The terminal step is judged by the buffer, the earlier ones only by their gap.
	TestTrue(TEXT("old first step, fresh second step"), MatchAt(*this, Sequence(10), TEXT("0:*A 9:*B"), 9));
	TestFalse(TEXT("fresh first step, old second step"), MatchAt(*this, Sequence(10), TEXT("0:*A 3:*B"), 12, [](FGSTN_MatchParams& Params) { Params.bHoldBuffer = false; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BufferDeterminismTest, "GSTNInput.Core.Input.Buffer.Determinism", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BufferDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const FGSTN_InputPattern Pat = Pattern({ Step({ Motion(EGSTN_DirectionSpace::Facing, { FacingBack, FacingForward }) }), Step({ Press(Attack()), Hold(Crouch()) }) });
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory History = PlayTo(*this, TEXT("0:+D 2:2t 5:8t 7:*A"), *Settings);

	FGSTN_PatternMatch First;
	FGSTN_PatternMatch Second;
	TestTrue(TEXT("matches"), MatchNow(Pat, History, *Settings, &First));
	TestTrue(TEXT("matches again"), MatchNow(Pat, History, *Settings, &Second));
	TestTrue(TEXT("same anchors"), First.StepAnchors == Second.StepAnchors);
	TestTrue(TEXT("same events"), First.UsedButtonEvents == Second.UsedButtonEvents);
	TestEqual(TEXT("same terminal press"), First.TerminalPressEvent, Second.TerminalPressEvent);

	// An invalid pattern never matches and never crashes.
	TestFalse(TEXT("empty pattern"), MatchNow(FGSTN_InputPattern(), History, *Settings));
	FGSTN_InputPattern NoAnchor = Pattern({ Hold(Crouch()) });
	FString Reason;
	TestFalse(TEXT("a step without anchor is invalid"), NoAnchor.IsStructurallyValid(&Reason));
	TestFalse(TEXT("and explains why"), Reason.IsEmpty());
	TestFalse(TEXT("and does not match"), MatchNow(NoAnchor, History, *Settings));
	TestTrue(TEXT("a normal pattern is valid"), Pat.IsStructurallyValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
