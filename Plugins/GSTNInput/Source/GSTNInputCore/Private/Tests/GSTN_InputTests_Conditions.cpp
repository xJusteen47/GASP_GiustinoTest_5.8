#include "Tests/GSTN_TestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNTest;

namespace
{
	/** Plays the notation and checks whether the pattern matches at its last frame */
	void ExpectMatch(FAutomationTestBase& Test, const TCHAR* What, const FGSTN_InputPattern& Pattern, const TCHAR* Notation, bool bExpected, int32 Now = INDEX_NONE)
	{
		const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
		const FGSTN_InputHistory History = PlayTo(Test, Notation, *Settings, Now);
		Test.TestEqual(*FString::Printf(TEXT("%s [%s]"), What, Notation), MatchNow(Pattern, History, *Settings), bExpected);
	}

	/** Spin script: one stick token per angle, Spacing frames apart, then the press */
	FString SpinScript(const TArray<int32>& Angles, int32 Spacing)
	{
		FString Script;
		int32 Frame = 0;
		for (const int32 Angle : Angles)
		{
			Script += FString::Printf(TEXT("%d:@%d "), Frame, ((Angle % 360) + 360) % 360);
			Frame += Spacing;
		}
		Script += FString::Printf(TEXT("%d:+A"), Frame - Spacing + 1);
		return Script;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionPressTest, "GSTNInput.Core.Input.Conditions.Press", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionPressTest::RunTest(const FString& Parameters)
{
	const FGSTN_InputPattern PressA = Pattern({ Press(Attack()) });

	ExpectMatch(*this, TEXT("a press matches"), PressA, TEXT("5:+A"), true);
	ExpectMatch(*this, TEXT("a tap matches"), PressA, TEXT("5:*A"), true);
	ExpectMatch(*this, TEXT("another button does not"), PressA, TEXT("5:+B"), false);
	ExpectMatch(*this, TEXT("a released press that is too old is gone"), PressA, TEXT("0:+A 5:-A"), false, 30);
	ExpectMatch(*this, TEXT("any of two buttons (second)"), Pattern({ PressAny(Attack(), Strong()) }), TEXT("5:+B"), true);
	ExpectMatch(*this, TEXT("any of two buttons (neither)"), Pattern({ PressAny(Attack(), Strong()) }), TEXT("5:+C"), false);

	// A consumed press and a reserved press are ignored.
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A"), *Settings);
	FGSTN_PatternMatch Match;
	if (!TestTrue(TEXT("press found"), MatchNow(PressA, History, *Settings, &Match)))
	{
		return true;
	}
	TestEqual(TEXT("one event used"), Match.UsedButtonEvents.Num(), 1);
	TestEqual(TEXT("it is the terminal press"), Match.TerminalPressEvent, static_cast<int32>(Match.UsedButtonEvents[0]));
	TestEqual(TEXT("anchor is the press frame"), Match.StepAnchors[0], 5);
	TestFalse(TEXT("a reserved press is ignored"), MatchNow(PressA, History, *Settings, nullptr,
		[&Match](FGSTN_MatchParams& Params) { Params.ReservedEvents = 1u << Match.UsedButtonEvents[0]; }));
	History.Consume(Match.UsedButtonEvents[0]);
	TestFalse(TEXT("a consumed press is ignored"), MatchNow(PressA, History, *Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionReleaseTest, "GSTNInput.Core.Input.Conditions.Release", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionReleaseTest::RunTest(const FString& Parameters)
{
	const TCHAR* Held10 = TEXT("0:+A 10:-A");
	ExpectMatch(*this, TEXT("release after a 10 frame hold, minimum 0"), Pattern({ Release(Attack(), 0) }), Held10, true);
	ExpectMatch(*this, TEXT("minimum 10 holds"), Pattern({ Release(Attack(), 10) }), Held10, true);
	ExpectMatch(*this, TEXT("minimum 11 does not"), Pattern({ Release(Attack(), 11) }), Held10, false);
	ExpectMatch(*this, TEXT("another button's release"), Pattern({ Release(Strong(), 0) }), Held10, false);
	ExpectMatch(*this, TEXT("a press alone is not a release"), Pattern({ Release(Attack(), 0) }), TEXT("0:+A"), false);
	ExpectMatch(*this, TEXT("a tap is a zero length hold"), Pattern({ Release(Attack(), 1) }), TEXT("4:*A"), false);
	ExpectMatch(*this, TEXT("a tap passes minimum 0"), Pattern({ Release(Attack(), 0) }), TEXT("4:*A"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionHoldTest, "GSTNInput.Core.Input.Conditions.Hold", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionHoldTest::RunTest(const FString& Parameters)
{
	const auto AttackWithHold = [](int32 MinFrames) { return Pattern({ Step({ Press(Attack()), Hold(Crouch(), MinFrames) }) }); };

	ExpectMatch(*this, TEXT("modifier held before the press"), AttackWithHold(0), TEXT("0:+D 5:+A"), true);
	ExpectMatch(*this, TEXT("modifier held long enough"), AttackWithHold(5), TEXT("0:+D 5:+A"), true);
	ExpectMatch(*this, TEXT("modifier not held long enough"), AttackWithHold(6), TEXT("0:+D 5:+A"), false);
	ExpectMatch(*this, TEXT("no modifier"), AttackWithHold(0), TEXT("5:+A"), false);

	// The tolerance (3 frames) works on both sides of the press.
	ExpectMatch(*this, TEXT("modifier pressed 2 frames after the press"), AttackWithHold(0), TEXT("5:+A 7:+D"), true);
	ExpectMatch(*this, TEXT("modifier pressed 3 frames after the press"), AttackWithHold(0), TEXT("5:+A 8:+D"), true);
	ExpectMatch(*this, TEXT("modifier pressed 4 frames after the press"), AttackWithHold(0), TEXT("5:+A 9:+D"), false);
	ExpectMatch(*this, TEXT("modifier released 1 frame before the press"), AttackWithHold(0), TEXT("0:+D 4:-D 5:+A"), true);
	ExpectMatch(*this, TEXT("modifier released long before the press"), AttackWithHold(0), TEXT("0:+D 2:-D 10:+A"), false);
	ExpectMatch(*this, TEXT("a tap of the modifier is never down"), AttackWithHold(0), TEXT("5:*D 5:+A"), false);

	// A step without hold does not care.
	ExpectMatch(*this, TEXT("modifier does not use the press"), Pattern({ Press(Attack()) }), TEXT("0:+D 5:+A"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionDirectionTest, "GSTNInput.Core.Input.Conditions.Direction", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionDirectionTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const auto AttackWith = [](const FCondition& Direction) { return Pattern({ Step({ Press(Attack()), Direction }) }); };
	const FGSTN_DirectionMask Right = FromDirection(EDir::Right);

	// Facing, Stick and World agree with the default camera and character yaw.
	const FGSTN_InputPattern RightTilt = AttackWith(Direction(EGSTN_DirectionSpace::Facing, Right, EGSTN_StickZone::Tilt));
	ExpectMatch(*this, TEXT("right tilt"), RightTilt, TEXT("0:6t 5:+A"), true);
	ExpectMatch(*this, TEXT("wrong direction"), RightTilt, TEXT("0:4t 5:+A"), false);
	ExpectMatch(*this, TEXT("a light push is not a tilt"), RightTilt, TEXT("0:6a 5:+A"), false);
	ExpectMatch(*this, TEXT("neutral stick"), RightTilt, TEXT("5:+A"), false);

	// Neutral or lightly pushed: any direction, MaxZone Aim.
	const FGSTN_InputPattern Standing = AttackWith(Direction(EGSTN_DirectionSpace::Facing, All, EGSTN_StickZone::Neutral, EGSTN_StickZone::Aim));
	ExpectMatch(*this, TEXT("neutral stick is standing"), Standing, TEXT("5:+A"), true);
	ExpectMatch(*this, TEXT("a light push is standing"), Standing, TEXT("0:8a 5:+A"), true);
	ExpectMatch(*this, TEXT("a tilt is not standing"), Standing, TEXT("0:8t 5:+A"), false);

	// The tolerance reaches the frames around the press.
	ExpectMatch(*this, TEXT("tilt 3 frames before the press"), RightTilt, TEXT("2:6t 3:5 5:+A"), true);
	ExpectMatch(*this, TEXT("tilt 4 frames before the press"), RightTilt, TEXT("1:6t 2:5 5:+A"), false);
	ExpectMatch(*this, TEXT("tilt 1 frame after the press"), RightTilt, TEXT("5:+A 6:6t"), true);

	// Spaces: the character faces 90 degrees, so a stick pushed right is forward for it.
	const FGSTN_InputPattern Forward = AttackWith(Direction(EGSTN_DirectionSpace::Facing, FromDirection(EDir::Up), EGSTN_StickZone::Tilt));
	const FGSTN_InputPattern StickRight = AttackWith(Direction(EGSTN_DirectionSpace::Stick, Right, EGSTN_StickZone::Tilt));
	ExpectMatch(*this, TEXT("facing space follows the character"), Forward, TEXT("0:face=90 0:6t 5:+A"), true);
	ExpectMatch(*this, TEXT("stick space ignores the character"), StickRight, TEXT("0:face=90 0:6t 5:+A"), true);
	ExpectMatch(*this, TEXT("stick space ignores the camera"), StickRight, TEXT("0:cam=90 0:6t 5:+A"), true);
	ExpectMatch(*this, TEXT("facing up is not stick up when the character turned"), Forward, TEXT("0:face=90 0:8t 5:+A"), false);

	// Flick.
	const FGSTN_InputPattern Flick = AttackWith(Direction(EGSTN_DirectionSpace::Facing, Right, EGSTN_StickZone::Tilt, EGSTN_StickZone::Tilt, true));
	ExpectMatch(*this, TEXT("flick"), Flick, TEXT("4:6t! 5:+A"), true);
	ExpectMatch(*this, TEXT("a held tilt is not a flick"), Flick, TEXT("0:6t 9:+A"), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionMotionTest, "GSTNInput.Core.Input.Conditions.Motion", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionMotionTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const FGSTN_DirectionMask Back = FromDirection(EDir::Down);
	const FGSTN_DirectionMask Forward = FromDirection(EDir::Up);
	const auto MotionThenAttack = [](const FCondition& Motion) { return Pattern({ Step({ Motion }), Step({ Press(Attack()) }) }); };

	const FGSTN_InputPattern BackForward = MotionThenAttack(Motion(EGSTN_DirectionSpace::Facing, { Back, Forward }));
	ExpectMatch(*this, TEXT("back then forward then attack"), BackForward, TEXT("0:2t 4:8t 6:+A"), true);
	ExpectMatch(*this, TEXT("forward then back does not"), BackForward, TEXT("0:8t 4:2t 6:+A"), false);
	ExpectMatch(*this, TEXT("only forward"), BackForward, TEXT("0:8t 6:+A"), false);
	ExpectMatch(*this, TEXT("noise between the elements is allowed"), BackForward, TEXT("0:2t 2:1t 3:4t 4:8t 6:+A"), true);
	ExpectMatch(*this, TEXT("a light push is not an element"), BackForward, TEXT("0:2t 4:8a 6:+A"), false);

	// Limits: 12 frames between elements, 30 for the whole motion.
	ExpectMatch(*this, TEXT("elements 12 frames apart"), BackForward, TEXT("0:2t 12:8t 13:+A"), true);
	ExpectMatch(*this, TEXT("elements 13 frames apart"), BackForward, TEXT("0:2t 13:8t 14:+A"), false);
	const FGSTN_InputPattern ThreeElements = MotionThenAttack(Motion(EGSTN_DirectionSpace::Facing,
		{ FromDirection(EDir::Down), FromDirection(EDir::DownRight), FromDirection(EDir::Right) }, 12, 10));
	ExpectMatch(*this, TEXT("quarter circle inside the total limit"), ThreeElements, TEXT("0:2t 4:3t 8:6t 9:+A"), true);
	ExpectMatch(*this, TEXT("quarter circle over the total limit"), ThreeElements, TEXT("0:2t 6:3t 12:6t 13:+A"), false);

	// The attack must follow the motion within the gap of the motion's step (15 frames).
	ExpectMatch(*this, TEXT("attack 15 frames after the motion"), BackForward, TEXT("0:2t 4:8t 19:+A"), true);
	ExpectMatch(*this, TEXT("attack 16 frames after the motion"), BackForward, TEXT("0:2t 4:8t 20:+A"), false);

	// A set of directions: the whole back arc.
	const FGSTN_InputPattern BackArc = MotionThenAttack(Motion(EGSTN_DirectionSpace::Facing, { FacingBack, Forward }));
	ExpectMatch(*this, TEXT("back diagonal counts as back"), BackArc, TEXT("0:1t 4:8t 6:+A"), true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionSpinTest, "GSTNInput.Core.Input.Conditions.Spin", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionSpinTest::RunTest(const FString& Parameters)
{
	const auto SpinThenAttack = [](EGSTN_SpinRotation Rotation, int32 MaxFrames = 30, int32 MaxSkipped = 1)
	{
		return Pattern({ Step({ Spin(Rotation, MaxFrames, MaxSkipped) }), Step({ Press(Attack()) }) });
	};

	// A full turn from every starting sector, in both senses.
	for (int32 Start = 0; Start < 8; ++Start)
	{
		for (const bool bClockwise : { true, false })
		{
			TArray<int32> Angles;
			for (int32 Step = 0; Step <= 8; ++Step)
			{
				Angles.Add((Start + (bClockwise ? Step : -Step)) * 45);
			}
			const FString Script = SpinScript(Angles, 2);
			const FString Name = FString::Printf(TEXT("%s spin from sector %d"), bClockwise ? TEXT("clockwise") : TEXT("counter-clockwise"), Start);
			ExpectMatch(*this, *(Name + TEXT(" is accepted by Either")), SpinThenAttack(EGSTN_SpinRotation::Either), *Script, true);
			ExpectMatch(*this, *(Name + TEXT(" in the right sense")), SpinThenAttack(bClockwise ? EGSTN_SpinRotation::Clockwise : EGSTN_SpinRotation::CounterClockwise), *Script, true);
			ExpectMatch(*this, *(Name + TEXT(" in the wrong sense")), SpinThenAttack(bClockwise ? EGSTN_SpinRotation::CounterClockwise : EGSTN_SpinRotation::Clockwise), *Script, false);
		}
	}

	// 315 degrees is not a full turn.
	ExpectMatch(*this, TEXT("315 degrees"), SpinThenAttack(EGSTN_SpinRotation::Either), *SpinScript({ 0, 45, 90, 135, 180, 225, 270, 315 }, 2), false);

	// A fast stick may skip one sector between events.
	const FString Skipping = SpinScript({ 0, 45, 90, 135, 225, 270, 315, 0 }, 2);
	ExpectMatch(*this, TEXT("one skipped sector"), SpinThenAttack(EGSTN_SpinRotation::Either, 30, 1), *Skipping, true);
	ExpectMatch(*this, TEXT("skipping not allowed"), SpinThenAttack(EGSTN_SpinRotation::Either, 30, 0), *Skipping, false);

	// Time: 8 steps 3 frames apart fit 30 frames, 5 frames apart do not.
	const TArray<int32> Turn = { 0, 45, 90, 135, 180, 225, 270, 315, 360 };
	ExpectMatch(*this, TEXT("a spin inside the time limit"), SpinThenAttack(EGSTN_SpinRotation::Either), *SpinScript(Turn, 3), true);
	ExpectMatch(*this, TEXT("a spin slower than the limit"), SpinThenAttack(EGSTN_SpinRotation::Either), *SpinScript(Turn, 5), false);

	// A change of sense in the middle breaks the chain.
	ExpectMatch(*this, TEXT("reversing"), SpinThenAttack(EGSTN_SpinRotation::Either), *SpinScript({ 0, 45, 90, 135, 90, 45, 0, 315, 270 }, 2), false);
	return true;
}

// ---- Spin and drill D0 spike (spin-drill-plan.md §2): the Spin condition as the spin command, stick-only slots.

namespace
{
	/** Stick-only spin slot, as the spin command uses it (spin-drill.md §5): no button after the rotation */
	FGSTN_InputPattern SpinOnly(EGSTN_SpinRotation Rotation, float MinDegrees = 360.f, int32 MaxFrames = 30, int32 MaxSkipped = 1)
	{
		return Pattern({ Spin(Rotation, MaxFrames, MaxSkipped, MinDegrees) });
	}

	/** The stick at an angle (0 = up, clockwise) from a frame on */
	struct FStickToken
	{
		int32 Frame = 0;
		int32 Angle = 0;
	};

	/** Stick tokens at the given frames and angles, nothing else; Prefix may set the camera and facing first */
	FString StickPath(const TArray<FStickToken>& Tokens, const TCHAR* Prefix = TEXT(""))
	{
		FString Script = Prefix;
		for (const FStickToken& Token : Tokens)
		{
			Script += FString::Printf(TEXT(" %d:@%d"), Token.Frame, ((Token.Angle % 360) + 360) % 360);
		}
		return Script.TrimStart();
	}

	/** A stick turning DegreesPerFrame every frame (negative = counter-clockwise) until it has turned TotalDegrees */
	FString StickSweep(int32 DegreesPerFrame, int32 TotalDegrees, const TCHAR* Prefix = TEXT(""))
	{
		TArray<FStickToken> Tokens;
		const int32 Frames = TotalDegrees / FMath::Abs(DegreesPerFrame);
		for (int32 Frame = 0; Frame <= Frames; ++Frame)
		{
			Tokens.Add({ Frame, Frame * DegreesPerFrame });
		}
		return StickPath(Tokens, Prefix);
	}

	/** The keyboard sweep A-S-D as a sequence of pure directions (input.md §9): left, down, right, other events in between allowed */
	FGSTN_InputPattern LeftDownRightMotion()
	{
		using namespace GSTN_DirectionMasks;
		return Pattern({ Motion(EGSTN_DirectionSpace::Stick, { FromDirection(EDir::Left), FromDirection(EDir::Down), FromDirection(EDir::Right) }) });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionSpinStickSpeedsTest, "GSTNInput.Core.Input.Conditions.SpinStickSpeeds", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionSpinStickSpeedsTest::RunTest(const FString& Parameters)
{
	// Default leniency: 360 degrees in 30 frames (480 ms at the 62.5 Hz fixed tick), one skipped sector per event.
	for (const bool bClockwise : { true, false })
	{
		const int32 Sign = bClockwise ? 1 : -1;
		const EGSTN_SpinRotation Right = bClockwise ? EGSTN_SpinRotation::Clockwise : EGSTN_SpinRotation::CounterClockwise;
		const EGSTN_SpinRotation Wrong = bClockwise ? EGSTN_SpinRotation::CounterClockwise : EGSTN_SpinRotation::Clockwise;
		const FString Sense = bClockwise ? TEXT("clockwise") : TEXT("counter-clockwise");

		for (const int32 Spacing : { 1, 2, 3 })
		{
			TArray<FStickToken> Tokens;
			for (int32 Step = 0; Step <= 8; ++Step)
			{
				Tokens.Add({ Step * Spacing, Sign * Step * 45 });
			}
			const FString Script = StickPath(Tokens);
			const FString Name = FString::Printf(TEXT("%s turn, a sector every %d frames"), *Sense, Spacing);
			ExpectMatch(*this, *(Name + TEXT(", right sense")), SpinOnly(Right), *Script, true);
			ExpectMatch(*this, *(Name + TEXT(", wrong sense")), SpinOnly(Wrong), *Script, false);
		}

		// The slowest turn accepted spans exactly MaxFrames from the first sector to the last.
		const TArray<int32> Slowest = { 0, 4, 8, 12, 16, 20, 24, 27, 30 };
		const TArray<int32> TooSlow = { 0, 4, 8, 12, 16, 20, 24, 28, 31 };
		TArray<FStickToken> SlowestTokens;
		TArray<FStickToken> TooSlowTokens;
		for (int32 Step = 0; Step <= 8; ++Step)
		{
			SlowestTokens.Add({ Slowest[Step], Sign * Step * 45 });
			TooSlowTokens.Add({ TooSlow[Step], Sign * Step * 45 });
		}
		ExpectMatch(*this, *(Sense + TEXT(" turn over 30 frames")), SpinOnly(Right), *StickPath(SlowestTokens), true);
		ExpectMatch(*this, *(Sense + TEXT(" turn over 31 frames")), SpinOnly(Right), *StickPath(TooSlowTokens), false);

		// A very fast stick seen every other sector, one frame apart: four 90 degree steps.
		ExpectMatch(*this, *(Sense + TEXT(" turn in 4 frames, every other sector")), SpinOnly(Right),
			*StickPath({ { 0, 0 }, { 1, Sign * 90 }, { 2, Sign * 180 }, { 3, Sign * 270 }, { 4, Sign * 360 } }), true);

		// Faster still (135 degrees between samples) is beyond the default leniency.
		ExpectMatch(*this, *(Sense + TEXT(" two sectors skipped per frame")), SpinOnly(Right),
			*StickPath({ { 0, 0 }, { 1, Sign * 135 }, { 2, Sign * 270 }, { 3, Sign * 405 } }), false);

		// A continuous analog sweep, angles between sector centres, 15 degrees a frame (24 frames, 384 ms).
		ExpectMatch(*this, *(Sense + TEXT(" continuous sweep")), SpinOnly(Right), *StickSweep(Sign * 15, 360), true);
	}

	// A one-sector wobble back in the middle breaks the chain: the full turn has to be made after it.
	ExpectMatch(*this, TEXT("wobble mid-turn"), SpinOnly(EGSTN_SpinRotation::Either),
		*StickPath({ { 0, 0 }, { 2, 45 }, { 4, 90 }, { 6, 135 }, { 8, 180 }, { 10, 135 }, { 12, 180 }, { 14, 225 }, { 16, 270 }, { 18, 315 }, { 20, 360 } }), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionSpinDirectionRingTest, "GSTNInput.Core.Input.Conditions.SpinDirectionRingLimit", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionSpinDirectionRingTest::RunTest(const FString& Parameters)
{
	// The history pushes a direction event when the 8-way direction changes in ANY space (Stick, World, Facing) and keeps
	// 16 of them. With the camera and the character both at 0 the three spaces change on the same frames: 9 events a turn.
	for (const int32 DegreesPerFrame : { 15, -15 })
	{
		const EGSTN_SpinRotation Sense = DegreesPerFrame > 0 ? EGSTN_SpinRotation::Clockwise : EGSTN_SpinRotation::CounterClockwise;
		const FString Name = DegreesPerFrame > 0 ? TEXT("clockwise") : TEXT("counter-clockwise");
		ExpectMatch(*this, *(Name + TEXT(" sweep, camera and facing aligned")), SpinOnly(Sense), *StickSweep(DegreesPerFrame, 360), true);

		// Known limit (D0 finding, spin-drill-spike-report.md): camera at 15 and character at 30 put the World and Facing
		// boundaries 15 degrees away from the Stick ones, so the same turn makes 25 events. The ring drops the oldest and
		// the spin sees less than 360 degrees. Flip to true when the spin gets its own stick-space log or a bigger ring.
		ExpectMatch(*this, *(Name + TEXT(" sweep, camera 15 facing 30 (known limit)")), SpinOnly(Sense),
			*StickSweep(DegreesPerFrame, 360, TEXT("0:cam=15 0:face=30")), false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionSpinKeyboardSweepTest, "GSTNInput.Core.Input.Conditions.SpinKeyboardSweep", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionSpinKeyboardSweepTest::RunTest(const FString& Parameters)
{
	// 180 degree slots, otherwise default leniency. Keys as numpad stick tokens: A = 4, A+S = 1, S = 2, S+D = 3, D = 6.
	const FGSTN_InputPattern Ccw180 = SpinOnly(EGSTN_SpinRotation::CounterClockwise, 180.f);
	const FGSTN_InputPattern Cw180 = SpinOnly(EGSTN_SpinRotation::Clockwise, 180.f);

	// A-S-D rolled with both overlaps (left, down-left, down, down-right, right) is 180 counter-clockwise.
	const TCHAR* Asd = TEXT("0:4 3:1 6:2 9:3 12:6");
	ExpectMatch(*this, TEXT("A-S-D rolled, counter-clockwise slot"), Ccw180, Asd, true);
	ExpectMatch(*this, TEXT("A-S-D rolled, clockwise slot"), Cw180, Asd, false);

	// D-S-A is the clockwise one.
	const TCHAR* Dsa = TEXT("0:6 3:3 6:2 9:1 12:4");
	ExpectMatch(*this, TEXT("D-S-A rolled, clockwise slot"), Cw180, Dsa, true);
	ExpectMatch(*this, TEXT("D-S-A rolled, counter-clockwise slot"), Ccw180, Dsa, false);

	// Without the diagonal frames each key change is 90 degrees: one skipped sector, still accepted.
	ExpectMatch(*this, TEXT("A-S-D without overlaps"), Ccw180, TEXT("0:4 4:2 8:6"), true);
	ExpectMatch(*this, TEXT("A-S-D with one overlap"), Ccw180, TEXT("0:4 3:1 6:2 10:6"), true);
	ExpectMatch(*this, TEXT("A-S-D one frame per key state"), Ccw180, TEXT("0:4 1:1 2:2 3:3 4:6"), true);
	ExpectMatch(*this, TEXT("A-S-D over 30 frames"), Ccw180, TEXT("0:4 8:1 15:2 22:3 30:6"), true);
	ExpectMatch(*this, TEXT("A-S-D over 31 frames"), Ccw180, TEXT("0:4 8:1 15:2 22:3 31:6"), false);

	// The default 360 slot does not take the half sweep: the keyboard shortcut needs its own slot (or DigitalMinDegrees).
	ExpectMatch(*this, TEXT("A-S-D on a 360 slot"), SpinOnly(EGSTN_SpinRotation::Either), Asd, false);

	// All four keys rolled around are a 360 on the default slot.
	ExpectMatch(*this, TEXT("W-D-S-A-W rolled, 360 slot"), SpinOnly(EGSTN_SpinRotation::Clockwise), TEXT("0:8 2:9 4:6 6:3 8:2 10:1 12:4 14:7 16:8"), true);

	// Needs a change (D0 finding): a frame with no key between two keys is neutral and breaks the chain.
	const TCHAR* AsdWithGaps = TEXT("0:4 4:5 5:2 9:5 10:6");
	ExpectMatch(*this, TEXT("A-S-D with neutral gaps"), Ccw180, AsdWithGaps, false);

	// Needs a change or a lower threshold: A and S pressed on the same frame start on the diagonal, 135 degrees only.
	const TCHAR* AsdFromDiagonal = TEXT("0:1 3:2 6:3 9:6");
	ExpectMatch(*this, TEXT("A+S together then D, 180 slot"), Ccw180, AsdFromDiagonal, false);
	ExpectMatch(*this, TEXT("A+S together then D, 135 slot"), SpinOnly(EGSTN_SpinRotation::CounterClockwise, 135.f), AsdFromDiagonal, true);

	// The alternative with no condition change: a Motion left, down, right in Stick space takes the gaps too.
	const FGSTN_InputPattern Ldr = LeftDownRightMotion();
	ExpectMatch(*this, TEXT("motion: A-S-D rolled"), Ldr, Asd, true);
	ExpectMatch(*this, TEXT("motion: A-S-D without overlaps"), Ldr, TEXT("0:4 4:2 8:6"), true);
	ExpectMatch(*this, TEXT("motion: A-S-D with neutral gaps"), Ldr, AsdWithGaps, true);
	ExpectMatch(*this, TEXT("motion: D-S-A is not A-S-D"), Ldr, Dsa, false);
	ExpectMatch(*this, TEXT("motion: A+S together has no pure left"), Ldr, AsdFromDiagonal, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionSpinKeyboardFalsePositivesTest, "GSTNInput.Core.Input.Conditions.SpinKeyboardFalsePositives", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionSpinKeyboardFalsePositivesTest::RunTest(const FString& Parameters)
{
	const FGSTN_InputPattern Either180 = SpinOnly(EGSTN_SpinRotation::Either, 180.f);
	const FGSTN_InputPattern Ldr = LeftDownRightMotion();

	// Strafing: left to right is a 180 degree jump in one event (four sectors), never a rotation.
	const TCHAR* Strafe = TEXT("0:4 6:6 12:4 18:6 24:4");
	ExpectMatch(*this, TEXT("strafe left-right"), Either180, Strafe, false);
	ExpectMatch(*this, TEXT("strafe left-right through neutral"), Either180, TEXT("0:4 5:5 6:6 11:5 12:4 17:5 18:6"), false);
	ExpectMatch(*this, TEXT("strafe from down-left to down-right"), Either180, TEXT("0:1 6:3 12:1"), false);
	ExpectMatch(*this, TEXT("zig-zag while holding S"), Either180, TEXT("0:1 4:2 8:3 12:2 16:1 20:2 24:3"), false);
	ExpectMatch(*this, TEXT("motion: strafe"), Ldr, Strafe, false);
	ExpectMatch(*this, TEXT("motion: zig-zag while holding S"), Ldr, TEXT("0:1 4:2 8:3 12:2 16:1 20:2 24:3"), false);

	// Dash flicks: a double tap and a turn-around flick.
	ExpectMatch(*this, TEXT("double tap right"), Either180, TEXT("0:6 3:5 5:6!"), false);
	ExpectMatch(*this, TEXT("forward then flick back"), Either180, TEXT("0:8 6:5 7:2!"), false);
	ExpectMatch(*this, TEXT("forward then sidestep right"), Either180, TEXT("0:8 6:9 12:6"), false);

	// Known risk (D0 finding): a quick U-turn through the diagonals (W, W+D, D, D+S, S) IS a 180 clockwise rotation, so a
	// 180 slot fires on it; the same holds for a half circle on a gamepad stick. The Motion form does not.
	const TCHAR* UTurn = TEXT("0:8 2:9 4:6 6:3 8:2");
	ExpectMatch(*this, TEXT("U-turn through diagonals, 180 slot (known risk)"), Either180, UTurn, true);
	ExpectMatch(*this, TEXT("motion: U-turn through diagonals"), Ldr, UTurn, false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionChordTest, "GSTNInput.Core.Input.Conditions.Chord", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionChordTest::RunTest(const FString& Parameters)
{
	const FGSTN_InputPattern AttackSpecial = Pattern({ Chord(Attack(), Special(), 2) });

	ExpectMatch(*this, TEXT("same frame"), AttackSpecial, TEXT("5:+A 5:+C"), true);
	ExpectMatch(*this, TEXT("two frames apart"), AttackSpecial, TEXT("5:+A 7:+C"), true);
	ExpectMatch(*this, TEXT("three frames apart"), AttackSpecial, TEXT("5:+A 8:+C"), false);
	ExpectMatch(*this, TEXT("order does not matter"), AttackSpecial, TEXT("5:+C 6:+A"), true);
	ExpectMatch(*this, TEXT("one button only"), AttackSpecial, TEXT("5:+A"), false);
	ExpectMatch(*this, TEXT("the wrong second button"), AttackSpecial, TEXT("5:+A 5:+B"), false);

	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputHistory History = PlayTo(*this, TEXT("5:+A 6:+C"), *Settings);
	FGSTN_PatternMatch Match;
	if (!TestTrue(TEXT("chord found"), MatchNow(AttackSpecial, History, *Settings, &Match)))
	{
		return true;
	}
	TestEqual(TEXT("both presses used"), Match.UsedButtonEvents.Num(), 2);
	TestEqual(TEXT("anchor is the newest press"), Match.StepAnchors[0], 6);
	TestEqual(TEXT("the terminal press is the newest one"), History.GetButtonEvent(Match.TerminalPressEvent).Frame, 6);

	// One consumed press breaks the chord.
	History.Consume(Match.UsedButtonEvents[0]);
	TestFalse(TEXT("a consumed press breaks the chord"), MatchNow(AttackSpecial, History, *Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionChargeTest, "GSTNInput.Core.Input.Conditions.Charge", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionChargeTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const FGSTN_InputPattern ChargeBack = Pattern({ Step({ Press(Attack()), Charge(EGSTN_DirectionSpace::Facing, FromDirection(EDir::Down), 10, 3) }) });

	ExpectMatch(*this, TEXT("charged then released, press right after"), ChargeBack, TEXT("0:2t 12:5 14:+A"), true);
	ExpectMatch(*this, TEXT("charge kept while pressing"), ChargeBack, TEXT("0:2t 14:+A"), true);
	ExpectMatch(*this, TEXT("charge too short"), ChargeBack, TEXT("0:2t 5:5 7:+A"), false);
	ExpectMatch(*this, TEXT("released too early"), ChargeBack, TEXT("0:2t 12:5 20:+A"), false);
	ExpectMatch(*this, TEXT("no charge"), ChargeBack, TEXT("5:+A"), false);
	ExpectMatch(*this, TEXT("wrong direction"), ChargeBack, TEXT("0:8t 12:5 14:+A"), false);
	ExpectMatch(*this, TEXT("a light push does not charge"), ChargeBack, TEXT("0:2a 12:5 14:+A"), false);
	ExpectMatch(*this, TEXT("a charge of a different direction does not count after a switch"), ChargeBack, TEXT("0:2t 6:8t 14:+A"), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionMashTest, "GSTNInput.Core.Input.Conditions.Mash", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionMashTest::RunTest(const FString& Parameters)
{
	const TCHAR* Three = TEXT("0:*A 3:*A 6:*A");
	ExpectMatch(*this, TEXT("three presses in time"), Pattern({ Mash(Attack(), 3, 10) }), Three, true);
	ExpectMatch(*this, TEXT("three presses, four wanted"), Pattern({ Mash(Attack(), 4, 10) }), Three, false);
	ExpectMatch(*this, TEXT("three presses too spread out"), Pattern({ Mash(Attack(), 3, 5) }), Three, false);
	ExpectMatch(*this, TEXT("other button"), Pattern({ Mash(Strong(), 3, 10) }), Three, false);

	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory History = PlayTo(*this, Three, *Settings);
	FGSTN_PatternMatch Match;
	if (!TestTrue(TEXT("mash found"), MatchNow(Pattern({ Mash(Attack(), 3, 10) }), History, *Settings, &Match)))
	{
		return true;
	}
	TestEqual(TEXT("three presses used"), Match.UsedButtonEvents.Num(), 3);
	TestEqual(TEXT("anchor is the newest press"), Match.StepAnchors[0], 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionAnyOfTest, "GSTNInput.Core.Input.Conditions.AnyOf", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionAnyOfTest::RunTest(const FString& Parameters)
{
	FGSTN_InputCondition_AnyOf AnyOf;
	AnyOf.Conditions.Add(Press(Attack()));
	AnyOf.Conditions.Add(Press(Strong()));
	const FGSTN_InputPattern Pattern = GSTNTest::Pattern({ MakeCondition(AnyOf) });

	ExpectMatch(*this, TEXT("first child"), Pattern, TEXT("5:+A"), true);
	ExpectMatch(*this, TEXT("second child"), Pattern, TEXT("5:+B"), true);
	ExpectMatch(*this, TEXT("neither"), Pattern, TEXT("5:+C"), false);

	// The anchor is the newest candidate among the children, not the first child's.
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_InputHistory History = PlayTo(*this, TEXT("2:*A 4:*B"), *Settings);
	FGSTN_PatternMatch Match;
	if (!TestTrue(TEXT("found"), MatchNow(Pattern, History, *Settings, &Match)))
	{
		return true;
	}
	TestEqual(TEXT("newest child wins"), Match.StepAnchors[0], 4);

	TestTrue(TEXT("anchor like its first child"), AnyOf.IsAnchorCondition());
	TestEqual(TEXT("specificity of its strongest child"), AnyOf.GetSpecificity(), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionSpecificityTest, "GSTNInput.Core.Input.Conditions.Specificity", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionSpecificityTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const auto Specificity = [](const FCondition& Condition) { return Condition.GetPtr()->GetSpecificity(); };

	TestEqual(TEXT("press"), Specificity(Press(Attack())), 10);
	TestEqual(TEXT("release"), Specificity(Release(Attack())), 10);
	TestEqual(TEXT("hold"), Specificity(Hold(Crouch())), 5);
	TestEqual(TEXT("chord of two"), Specificity(Chord(Attack(), Special())), 20);
	TestEqual(TEXT("mash of three"), Specificity(Mash(Attack(), 3, 10)), 15);
	TestEqual(TEXT("direction"), Specificity(Direction(EGSTN_DirectionSpace::Facing, All)), 5);
	TestEqual(TEXT("tilted direction"), Specificity(Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt)), 8);
	TestEqual(TEXT("tilted flick"), Specificity(Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt, EGSTN_StickZone::Tilt, true)), 10);
	TestEqual(TEXT("motion of two"), Specificity(Motion(EGSTN_DirectionSpace::Facing, { FacingBack, FacingForward })), 10);
	TestEqual(TEXT("spin of 360"), Specificity(Spin(EGSTN_SpinRotation::Either)), 40);
	TestEqual(TEXT("charge"), Specificity(Charge(EGSTN_DirectionSpace::Facing, FacingBack, 30, 8)), 15);

	// A pattern sums them: crouch + tilted direction + attack outranks tilted direction + attack outranks the bare attack.
	const FGSTN_InputPattern Bare = Pattern({ Press(Attack()) });
	const FGSTN_InputPattern Directional = Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) });
	const FGSTN_InputPattern Crouching = Pattern({ Press(Attack()), Hold(Crouch()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) });
	TestTrue(TEXT("directional outranks bare"), Directional.GetSpecificity() > Bare.GetSpecificity());
	TestTrue(TEXT("crouching outranks directional"), Crouching.GetSpecificity() > Directional.GetSpecificity());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ConditionCanonicalInputTest, "GSTNInput.Core.Input.Conditions.CanonicalInput", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ConditionCanonicalInputTest::RunTest(const FString& Parameters)
{
	using namespace GSTN_DirectionMasks;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const FGSTN_ButtonMap Map = GSTNTest::MakeButtonMap();

	struct FCase
	{
		const TCHAR* Name;
		FGSTN_InputPattern Pattern;
	};
	const FCase Cases[] = {
		{ TEXT("press"), Pattern({ Press(Attack()) }) },
		{ TEXT("release after a hold"), Pattern({ Release(Strong(), 6) }) },
		{ TEXT("chord"), Pattern({ Chord(Attack(), Special()) }) },
		{ TEXT("mash"), Pattern({ Mash(Attack(), 3, 10) }) },
		{ TEXT("crouch attack"), Pattern({ Press(Attack()), Hold(Crouch(), 4) }) },
		{ TEXT("standing attack"), Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, All, EGSTN_StickZone::Neutral, EGSTN_StickZone::Aim) }) },
		{ TEXT("forward tilt attack"), Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, AnyTilted, EGSTN_StickZone::Tilt) }) },
		{ TEXT("flick attack"), Pattern({ Press(Attack()), Direction(EGSTN_DirectionSpace::Facing, FacingForward, EGSTN_StickZone::Tilt, EGSTN_StickZone::Tilt, true) }) },
		{ TEXT("crouch tilt attack"), Pattern({ Press(Attack()), Hold(Crouch()), Direction(EGSTN_DirectionSpace::World, AnyTilted, EGSTN_StickZone::Tilt) }) },
		{ TEXT("motion"), Pattern({ Motion(EGSTN_DirectionSpace::Facing, { FacingBack, FacingForward }) }) },
		{ TEXT("motion then attack"), Pattern({ Step({ Motion(EGSTN_DirectionSpace::Facing, { FromDirection(EDir::Down), FromDirection(EDir::DownRight), FromDirection(EDir::Right) }) }), Step({ Press(Attack()) }) }) },
		{ TEXT("spin"), Pattern({ Spin(EGSTN_SpinRotation::Either) }) },
		{ TEXT("counter-clockwise spin then special"), Pattern({ Step({ Spin(EGSTN_SpinRotation::CounterClockwise) }), Step({ Press(Special()) }) }) },
		{ TEXT("charge attack"), Pattern({ Press(Attack()), Charge(EGSTN_DirectionSpace::Facing, FacingBack, 20, 4) }) },
		{ TEXT("double tap"), Pattern({ Step({ Press(Attack()) }), Step({ Press(Attack()) }) }) },
	};

	for (const FCase& Case : Cases)
	{
		FGSTN_InputScript Script(GSTNTest::ButtonTags());
		int32 Frame = 20;
		Case.Pattern.AppendCanonicalInput(Script, Map, Frame);

		FGSTN_InputHistory History;
		TestTrue(*FString::Printf(TEXT("%s: canonical input plays"), Case.Name), Script.Advance(History, *Settings, Frame));
		TestTrue(*FString::Printf(TEXT("%s: the canonical input matches its own pattern (%s)"), Case.Name, *FGSTN_InputScript::Describe(History)),
			MatchNow(Case.Pattern, History, *Settings));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
