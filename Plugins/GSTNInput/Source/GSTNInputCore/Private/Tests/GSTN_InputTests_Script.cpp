#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using EDir = EGSTN_StickDirection;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ScriptNotationTest, "GSTNInput.Core.Input.Script.Notation", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ScriptNotationTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();

	// The example of the spec: stick left, a flick to the right and the attack, character turned 90 degrees.
	FGSTN_InputHistory History;
	int32 LastFrame = INDEX_NONE;
	TestTrue(TEXT("builds"), FGSTN_InputScript::Build(GSTNTest::ButtonTags(), TEXT("0:face=90 0:4t 6:6t! 7:+A"), *Settings, History, LastFrame));
	TestEqual(TEXT("last frame"), LastFrame, 7);
	TestEqual(TEXT("one press"), History.GetNumButtonEvents(), 1);
	TestEqual(TEXT("it is on frame 7"), History.GetButtonEvent(History.GetButtonSlot(0)).Frame, 7);
	TestEqual(TEXT("it is button 0"), static_cast<int32>(History.GetButtonEvent(History.GetButtonSlot(0)).Button), 0);

	// Left at 0; released at 5 (the flick leaves Neutral); Right with a flick at 6.
	TestEqual(TEXT("three stick events"), History.GetNumDirectionEvents(), 3);
	const FGSTN_StickSample AtStart = History.GetStickAt(0);
	TestEqual(TEXT("stick left"), AtStart.StickDir, EDir::Left);
	TestEqual(TEXT("stick left is the character's back when it faces 90"), AtStart.FacingDir, EDir::Down);
	TestEqual(TEXT("released on the frame before the flick"), History.GetStickAt(5).Zone, EGSTN_StickZone::Neutral);
	const FGSTN_StickSample Flick = History.GetStickAt(6);
	TestTrue(TEXT("the flick is a flick for the real classifier"), Flick.bFlick);
	TestEqual(TEXT("stick right"), Flick.StickDir, EDir::Right);
	TestEqual(TEXT("right is forward for a character facing 90"), Flick.FacingDir, EDir::Up);
	TestEqual(TEXT("the stick keeps its state"), History.GetStickAt(7).StickDir, EDir::Right);
	TestFalse(TEXT("the flick is gone at 7"), History.GetStickAt(7).bFlick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ScriptTokensTest, "GSTNInput.Core.Input.Script.Tokens", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ScriptTokensTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();
	const auto Build = [&Settings](const TCHAR* Notation)
	{
		FGSTN_InputHistory History;
		int32 LastFrame = 0;
		FGSTN_InputScript::Build(GSTNTest::ButtonTags(), Notation, *Settings, History, LastFrame);
		return History;
	};

	// Button names: letters, indices and tag names all give the same bit.
	TestEqual(TEXT("letter and tag name"), FGSTN_InputScript::Describe(Build(TEXT("1:+Attack 2:-A"))), FString(TEXT("1:+A 2:-A")));
	TestEqual(TEXT("crouch is the fourth button"), FGSTN_InputScript::Describe(Build(TEXT("1:+Crouch"))), FString(TEXT("1:+D")));
	TestEqual(TEXT("explicit index"), FGSTN_InputScript::Describe(Build(TEXT("1:+#7"))), FString(TEXT("1:+H")));
	TestEqual(TEXT("names ignore case"), FGSTN_InputScript::Describe(Build(TEXT("1:+jump"))), FString(TEXT("1:+F")));

	// A tap is a press and a release on the same frame.
	TestEqual(TEXT("tap"), FGSTN_InputScript::Describe(Build(TEXT("4:*B"))), FString(TEXT("4:+B 4:-B")));

	// Every numpad key.
	const struct { const TCHAR* Key; EDir Direction; } Keys[] = {
		{ TEXT("8"), EDir::Up }, { TEXT("9"), EDir::UpRight }, { TEXT("6"), EDir::Right }, { TEXT("3"), EDir::DownRight },
		{ TEXT("2"), EDir::Down }, { TEXT("1"), EDir::DownLeft }, { TEXT("4"), EDir::Left }, { TEXT("7"), EDir::UpLeft } };
	for (const auto& Key : Keys)
	{
		const FGSTN_InputHistory History = Build(*FString::Printf(TEXT("0:%st"), Key.Key));
		TestEqual(*FString::Printf(TEXT("numpad %s"), Key.Key), History.GetStickAt(0).StickDir, Key.Direction);
	}

	// Zones and exact angles.
	TestEqual(TEXT("light push is Aim"), Build(TEXT("0:8a")).GetStickAt(0).Zone, EGSTN_StickZone::Aim);
	TestEqual(TEXT("the default push is Tilt"), Build(TEXT("0:8")).GetStickAt(0).Zone, EGSTN_StickZone::Tilt);
	TestEqual(TEXT("5 is neutral"), Build(TEXT("0:5")).GetStickAt(0).Zone, EGSTN_StickZone::Neutral);
	TestEqual(TEXT("exact angle 45"), Build(TEXT("0:@45")).GetStickAt(0).StickDir, EDir::UpRight);
	TestEqual(TEXT("exact angle 100"), Build(TEXT("0:@100")).GetStickAt(0).StickDir, EDir::Right);
	TestEqual(TEXT("exact angle with a zone"), Build(TEXT("0:@200a")).GetStickAt(0).Zone, EGSTN_StickZone::Aim);

	// The camera directive moves the World direction only.
	const FGSTN_StickSample Camera = Build(TEXT("0:cam=90 0:8t")).GetStickAt(0);
	TestEqual(TEXT("stick space"), Camera.StickDir, EDir::Up);
	TestEqual(TEXT("world space"), Camera.WorldDir, EDir::Right);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ScriptErrorsTest, "GSTNInput.Core.Input.Script.Errors", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ScriptErrorsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();
	const TCHAR* BadScripts[] = {
		TEXT("nonsense"),
		TEXT("x:+A"),
		TEXT("-1:+A"),
		TEXT("3:"),
		TEXT("3:+Nope"),
		TEXT("3:??"),
		TEXT("3:0t"),
		TEXT("3:@abc"),
		TEXT("3:cam=left"),
		TEXT("0:6t!"),
		TEXT("3:5!"),
		TEXT("3:-A"),
		TEXT("1:+A 2:+A"),
		TEXT("1:*A 1:+A"),
	};

	for (const TCHAR* Script : BadScripts)
	{
		AddExpectedError(TEXT("Input script"), EAutomationExpectedErrorFlags::Contains, 0);
		FGSTN_InputHistory History;
		int32 LastFrame = 0;
		FString Error;
		TestFalse(*FString::Printf(TEXT("\"%s\" is rejected"), Script), FGSTN_InputScript::Build(GSTNTest::ButtonTags(), Script, *Settings, History, LastFrame, &Error));
		TestFalse(*FString::Printf(TEXT("\"%s\" explains why"), Script), Error.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ScriptAdvanceTest, "GSTNInput.Core.Input.Script.Advance", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ScriptAdvanceTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();
	const TCHAR* Notation = TEXT("0:cam=30 2:8t 4:+A 6:9t 8:+B 9:-A 12:3t 14:5 15:-B");

	FGSTN_InputScript Script(GSTNTest::ButtonTags());
	FString Error;
	TestTrue(TEXT("parses"), Script.Parse(Notation, Error));

	// Play half, fork the session (history and script are plain values), play the rest on both copies.
	FGSTN_InputHistory History;
	TestTrue(TEXT("first half"), Script.Advance(History, *Settings, 7));
	TestEqual(TEXT("next frame"), Script.GetNextFrame(), 8);
	FGSTN_InputHistory Fork = History;
	FGSTN_InputScript ForkScript = Script;
	TestTrue(TEXT("second half"), Script.Advance(History, *Settings, 20));
	TestTrue(TEXT("second half on the fork"), ForkScript.Advance(Fork, *Settings, 20));
	TestTrue(TEXT("both copies end equal"), History == Fork);

	// And equal to playing in one go.
	FGSTN_InputScript Whole(GSTNTest::ButtonTags());
	FGSTN_InputHistory WholeHistory;
	TestTrue(TEXT("whole script"), Whole.Parse(Notation, Error) && Whole.Advance(WholeHistory, *Settings, 20));
	TestTrue(TEXT("split and whole runs are equal"), History == WholeHistory);

	// Events can be added while playing, for frames not played yet.
	FGSTN_InputScript Live(GSTNTest::ButtonTags());
	FGSTN_InputHistory LiveHistory;
	Live.AddPress(2, 1u);
	TestTrue(TEXT("first frames"), Live.Advance(LiveHistory, *Settings, 3));
	Live.AddRelease(5, 1u);
	TestTrue(TEXT("later frames"), Live.Advance(LiveHistory, *Settings, 6));
	TestEqual(TEXT("press and release recorded"), LiveHistory.GetNumButtonEvents(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ScriptDescribeTest, "GSTNInput.Core.Input.Script.Describe", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ScriptDescribeTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNTest::MakeSettings();
	FGSTN_InputHistory History;
	int32 LastFrame = 0;
	FGSTN_InputScript::Build(GSTNTest::ButtonTags(), TEXT("2:4a 5:+A 6:+C 9:-A 12:5"), *Settings, History, LastFrame);

	TestEqual(TEXT("history in notation"), FGSTN_InputScript::Describe(History), FString(TEXT("2:4a 5:+A 6:+C 9:-A 12:5")));
	TestEqual(TEXT("only the newest events"), FGSTN_InputScript::Describe(History, 2), FString(TEXT("9:-A 12:5")));
	TestEqual(TEXT("empty history"), FGSTN_InputScript::Describe(FGSTN_InputHistory()), FString());

	// The description plays back to the same events.
	FGSTN_InputHistory Replayed;
	FGSTN_InputScript::Build(GSTNTest::ButtonTags(), FGSTN_InputScript::Describe(History), *Settings, Replayed, LastFrame);
	TestEqual(TEXT("replayed description gives the same notation"), FGSTN_InputScript::Describe(Replayed), FGSTN_InputScript::Describe(History));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
