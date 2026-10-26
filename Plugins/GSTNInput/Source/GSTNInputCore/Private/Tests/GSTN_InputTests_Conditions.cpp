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
