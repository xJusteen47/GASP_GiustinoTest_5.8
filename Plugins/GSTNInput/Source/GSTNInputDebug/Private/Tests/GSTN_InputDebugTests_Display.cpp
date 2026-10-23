#include "Display/GSTN_InputDisplayLayout.h"
#include "Display/GSTN_InputDisplayMetrics.h"
#include "Display/GSTN_InputDisplayScript.h"
#include "Framework/Application/SlateApplication.h"
#include "GSTNInputDebugModule.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sources/GSTN_LocalInputSource.h"
#include "Tests/GSTN_InputDebugTestSupport.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SGSTN_InputDisplay.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Flags of every input display test: pure logic, runs in the editor and in headless commandlets */
#define GSTN_INPUTDEBUG_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

using namespace GSTNInputDisplayTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugLayoutSampleTest, "GSTNInput.Debug.InputDisplay.LayoutOfSampleHistory", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugLayoutSampleTest::RunTest(const FString& Parameters)
{
	// The sample of the notation: stick left with the character turned 90 degrees, then a flick to the right and the attack button
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	if (!Play(*this, TEXT("0:face=90 0:4t 6:6t! 7:+A"), *Settings, Data))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);

	// One column per frame, frames 0 to 7, oldest first: the newest frame is the last column
	TestEqual(TEXT("first frame"), Layout.FirstFrame, 0);
	TestEqual(TEXT("columns"), Layout.NumColumns, 8);
	TestEqual(TEXT("six button rows"), Layout.Buttons.Num(), 6);
	TestEqual(TEXT("a cell per column"), Layout.Buttons[0].Num(), 8);
	TestEqual(TEXT("frame to column"), Layout.FrameToColumn(7), 7);
	TestEqual(TEXT("a frame past the strip has no column"), Layout.FrameToColumn(8), (int32)INDEX_NONE);
	if (Layout.NumColumns != 8)
	{
		return false;
	}

	// Stick: physical direction and numpad key, facing-relative direction as the classifier made it
	TestTrue(TEXT("frame 0 physical direction"), Layout.Stick[0].StickDirection == EGSTN_StickDirection::Left);
	TestTrue(TEXT("frame 0 physical key"), Layout.Stick[0].StickKey == TEXT('4'));
	TestTrue(TEXT("frame 0 zone"), Layout.Stick[0].Zone == EGSTN_StickZone::Tilt);
	TestTrue(TEXT("frame 0 facing direction is the history's"), Layout.Stick[0].FacingDirection == Data.History.GetStickAt(0).FacingDir);
	TestTrue(TEXT("frame 0 facing key is the history's"), Layout.Stick[0].FacingKey == FGSTN_InputDisplayLayout::GetNumpadKey(Data.History.GetStickAt(0).FacingDir));
	TestTrue(TEXT("turned 90 degrees, left on the stick is not left of the character"), Layout.Stick[0].FacingKey != TEXT('4') && Layout.Stick[0].FacingDirection != EGSTN_StickDirection::Left);
	TestTrue(TEXT("stick held until its next event"), Layout.Stick[3].StickKey == TEXT('4') && !Layout.Stick[3].bEvent);
	TestTrue(TEXT("frame 5 neutral"), Layout.Stick[5].StickKey == TEXT('5') && Layout.Stick[5].bEvent && Layout.Stick[5].StickDirection == EGSTN_StickDirection::Neutral);
	TestTrue(TEXT("frame 6 physical key"), Layout.Stick[6].StickKey == TEXT('6') && Layout.Stick[6].StickDirection == EGSTN_StickDirection::Right);
	TestTrue(TEXT("frame 6 is the flick"), Layout.Stick[6].bFlick);
	TestFalse(TEXT("frame 7 is not"), Layout.Stick[7].bFlick);

	// Events with the gaps in notation: 0:4t, then 5 frames to the release of the stick, 1 to the flick, 1 to the press
	if (TestEqual(TEXT("four events"), Layout.Events.Num(), 4))
	{
		TestEqual(TEXT("event items"), FString::Join(TArray<FString>{ Layout.Events[0].Item, Layout.Events[1].Item, Layout.Events[2].Item, Layout.Events[3].Item }, TEXT(" ")), FString(TEXT("4t 5 6t! +A")));
		TestEqual(TEXT("gaps"), FString::Printf(TEXT("%d %d %d %d"), Layout.Events[0].Gap, Layout.Events[1].Gap, Layout.Events[2].Gap, Layout.Events[3].Gap), FString(TEXT("0 5 1 1")));
		TestEqual(TEXT("last event label is the gap in notation"), Layout.Events[3].GetLabel(), FString(TEXT("1:+A")));
		TestEqual(TEXT("last event column"), Layout.Events[3].Column, 7);
	}
	TestEqual(TEXT("the strip as a script"), Layout.ToNotation(), FString(TEXT("0:4t 5:5 6:6t! 7:+A")));

	// The tooltip is the one place that prints the numpad notation, with the direction in words
	TestEqual(TEXT("stick tooltip text"), FGSTN_InputDisplayLayout::DescribeStickForTooltip(Layout.Stick[0]), FString(TEXT("4t (left, tilt)")));
	TestEqual(TEXT("flick tooltip text"), FGSTN_InputDisplayLayout::DescribeStickForTooltip(Layout.Stick[6]), FString(TEXT("6t! (right, tilt, flick)")));

	// Buttons: A is pressed on the last frame, not consumed (nothing resolved it); nothing else moved
	TestTrue(TEXT("press of A on the last frame"), Layout.Buttons[0][7].Kind == EGSTN_InputCellKind::Press);
	TestFalse(TEXT("not consumed"), Layout.Buttons[0][7].bConsumed);
	TestTrue(TEXT("A before the press is empty"), Layout.Buttons[0][6].Kind == EGSTN_InputCellKind::None);
	TestTrue(TEXT("B is empty"), Layout.Buttons[1][7].Kind == EGSTN_InputCellKind::None);

	// The strip reproduces itself: playing it back gives the same events (the facing directive is not part of the strip)
	FGSTN_InputDisplayData Again;
	if (Play(*this, *Layout.ToNotation(), *Settings, Again))
	{
		const FGSTN_InputDisplayLayout Second = FGSTN_InputDisplayLayout::Build(Again);
		TestEqual(TEXT("replayed strip has the same notation"), Second.ToNotation(), Layout.ToNotation());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugButtonRowsTest, "GSTNInput.Debug.InputDisplay.ButtonRows", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugButtonRowsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	// A pressed at 2 and released at 5; tap of B at 7; C pressed at 9 and still held
	if (!Play(*this, TEXT("2:+A 5:-A 7:*B 9:+C"), *Settings, Data))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);
	if (!TestEqual(TEXT("columns"), Layout.NumColumns, 10))
	{
		return false;
	}
	const auto Kind = [&Layout](int32 Button, int32 Column) { return Layout.Buttons[Button][Column].Kind; };
	TestTrue(TEXT("A: nothing before the press"), Kind(0, 1) == EGSTN_InputCellKind::None);
	TestTrue(TEXT("A: press"), Kind(0, 2) == EGSTN_InputCellKind::Press);
	TestTrue(TEXT("A: hold"), Kind(0, 3) == EGSTN_InputCellKind::Hold && Kind(0, 4) == EGSTN_InputCellKind::Hold);
	TestTrue(TEXT("A: release"), Kind(0, 5) == EGSTN_InputCellKind::Release);
	TestTrue(TEXT("A: nothing after"), Kind(0, 6) == EGSTN_InputCellKind::None);
	TestTrue(TEXT("B: one tap, no hold"), Kind(1, 7) == EGSTN_InputCellKind::Tap && Kind(1, 6) == EGSTN_InputCellKind::None && Kind(1, 8) == EGSTN_InputCellKind::None);
	TestTrue(TEXT("C: press, still held at the newest frame"), Kind(2, 9) == EGSTN_InputCellKind::Press);

	// A held button keeps its bar up to the newest frame
	FGSTN_InputDisplayData Held;
	if (Play(*this, TEXT("1:+A 4:+B"), *Settings, Held))
	{
		const FGSTN_InputDisplayLayout HeldLayout = FGSTN_InputDisplayLayout::Build(Held);
		TestTrue(TEXT("A: held through the newest frame"), HeldLayout.Buttons[0][2].Kind == EGSTN_InputCellKind::Hold && HeldLayout.Buttons[0][4].Kind == EGSTN_InputCellKind::Hold);
		TestTrue(TEXT("B: pressed on the newest frame"), HeldLayout.Buttons[1][4].Kind == EGSTN_InputCellKind::Press);
		TestEqual(TEXT("items"), FString::Join(TArray<FString>{ HeldLayout.Events[0].Item, HeldLayout.Events[1].Item }, TEXT(" ")), FString(TEXT("+A +B")));
	}

	// The window shows the newest frames only
	FGSTN_InputDisplayLayoutParams Narrow;
	Narrow.MaxColumns = 4;
	const FGSTN_InputDisplayLayout Window = FGSTN_InputDisplayLayout::Build(Data, Narrow);
	TestEqual(TEXT("window columns"), Window.NumColumns, 4);
	TestEqual(TEXT("window starts at the oldest frame it shows"), Window.FirstFrame, 6);
	TestTrue(TEXT("a tap inside the window"), Window.Buttons[1][1].Kind == EGSTN_InputCellKind::Tap);
	TestEqual(TEXT("events outside the window are not listed"), Window.Events.Num(), 2);

	// A history that has had no frame yet has nothing to show
	const FGSTN_InputDisplayLayout Empty = FGSTN_InputDisplayLayout::Build(FGSTN_InputDisplayData());
	TestEqual(TEXT("no columns"), Empty.NumColumns, 0);
	TestEqual(TEXT("no notation"), Empty.ToNotation(), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugFlickTest, "GSTNInput.Debug.InputDisplay.FlickMarker", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugFlickTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	if (!Play(*this, TEXT("3:6t!"), *Settings, Data))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);
	if (!TestEqual(TEXT("columns"), Layout.NumColumns, 4))
	{
		return false;
	}
	for (int32 Column = 0; Column < 3; ++Column)
	{
		TestFalse(FString::Printf(TEXT("no flick on frame %d"), Column), Layout.Stick[Column].bFlick);
	}
	TestTrue(TEXT("flick on the frame the stick entered tilt"), Layout.Stick[3].bFlick);
	TestEqual(TEXT("its item carries the flick sign"), FGSTN_InputDisplayLayout::DescribeStick(Layout.Stick[3]), FString(TEXT("6t!")));
	TestTrue(TEXT("and is an event column"), Layout.Stick[3].bEvent);
	TestEqual(TEXT("the last event"), Layout.Events.Last().GetLabel(), FString(TEXT("0:6t!")));

	// Pushed through the light zone first, the same stick is a plain tilt: a flick is neutral to tilt in one go
	FGSTN_InputDisplayData Slow;
	if (Play(*this, TEXT("0:6a 3:6t"), *Settings, Slow))
	{
		const FGSTN_InputDisplayLayout Plain = FGSTN_InputDisplayLayout::Build(Slow);
		TestFalse(TEXT("no flick for a held tilt"), Plain.Stick[3].bFlick);
		TestEqual(TEXT("item"), FGSTN_InputDisplayLayout::DescribeStick(Plain.Stick[3]), FString(TEXT("6t")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugScriptTest, "GSTNInput.Debug.InputDisplay.ScriptPlaysWithoutACommandSet", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugScriptTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// With no slots (this module knows no game's commands) the notation is played and nothing resolves
	const FGSTN_InputDisplayScriptResult Result = FGSTN_InputDisplayScript::Resolve(TEXT("0:4t 6:6t! 7:+A"), ButtonTags(), TConstArrayView<const FGSTN_InputSlotSpec*>(), *Settings);
	TestTrue(TEXT("plays"), Result.bOk);
	TestEqual(TEXT("last frame"), Result.LastFrame, 7);
	TestFalse(TEXT("no command without a set"), Result.Command.IsValid());
	TestEqual(TEXT("no slot"), Result.SlotIndex, (int32)INDEX_NONE);
	TestEqual(TEXT("no marker"), Result.Data.Markers.Num(), 0);
	TestFalse(TEXT("nothing greyed"), FGSTN_InputDisplayLayout::Build(Result.Data).Buttons[0][7].bConsumed);

	// A typo is an answer, not a crash or a log error
	const FGSTN_InputDisplayScriptResult Bad = FGSTN_InputDisplayScript::Resolve(TEXT("banana"), ButtonTags(), TConstArrayView<const FGSTN_InputSlotSpec*>(), *Settings);
	TestFalse(TEXT("not a notation"), Bad.bOk);
	TestFalse(TEXT("with a reason"), Bad.Error.IsEmpty());
	TestEqual(TEXT("and no data"), Bad.Data.History.GetLatestFrame(), (int32)INDEX_NONE);
	TestFalse(TEXT("empty notation"), FGSTN_InputDisplayScript::Resolve(TEXT("  "), ButtonTags(), TConstArrayView<const FGSTN_InputSlotSpec*>(), *Settings).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugPureTest, "GSTNInput.Debug.InputDisplay.LayoutIsPure", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugPureTest::RunTest(const FString& Parameters)
{
	// The layout and the metrics are pure functions: read their files and look at what they include. No Slate, no widget, no world.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNInput"));
	if (!TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		return false;
	}
	const FString Source = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNInputDebug"));
	const TArray<FString> Files =
	{
		FPaths::Combine(Source, TEXT("Public/Display/GSTN_InputDisplayLayout.h")),
		FPaths::Combine(Source, TEXT("Private/Display/GSTN_InputDisplayLayout.cpp")),
		FPaths::Combine(Source, TEXT("Public/Display/GSTN_InputDisplayMetrics.h")),
		FPaths::Combine(Source, TEXT("Private/Display/GSTN_InputDisplayMetrics.cpp")),
		FPaths::Combine(Source, TEXT("Private/Sources/GSTN_InputTargetSelection.h")),
		FPaths::Combine(Source, TEXT("Private/Sources/GSTN_InputTargetSelection.cpp")),
	};
	for (const FString& File : Files)
	{
		FString Text;
		if (!TestTrue(FString::Printf(TEXT("%s can be read"), *File), FFileHelper::LoadFileToString(Text, *File)))
		{
			continue;
		}
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		int32 NumIncludes = 0;
		for (const FString& Line : Lines)
		{
			if (!Line.TrimStart().StartsWith(TEXT("#include")))
			{
				continue;
			}
			++NumIncludes;
			for (const TCHAR* Forbidden : { TEXT("Slate"), TEXT("Widgets/S"), TEXT("Framework/"), TEXT("Engine/World"), TEXT("GameFramework"), TEXT("Simulation/"), TEXT("Layout/"), TEXT("Combat") })
			{
				TestFalse(FString::Printf(TEXT("%s: \"%s\" does not include %s"), *FPaths::GetCleanFilename(File), *Line.TrimStartAndEnd(), Forbidden), Line.Contains(Forbidden));
			}
		}
		TestTrue(FString::Printf(TEXT("%s has includes to check"), *FPaths::GetCleanFilename(File)), NumIncludes > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugLocalSourceTest, "GSTNInput.Debug.InputDisplay.NoLocalPawnShowsNothing", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugLocalSourceTest::RunTest(const FString& Parameters)
{
	// No play session runs under the automation test: the source says there is nothing, and leaves the data alone
	FGSTN_LocalInputSource Source;
	FGSTN_InputDisplayData Data;
	TestFalse(TEXT("no local pawn, no data"), Source.Fetch(Data));
	TestFalse(TEXT("no source"), Source.GetSource().IsValid());
	TestFalse(TEXT("and it says why"), Source.GetStatusText().IsEmpty());
	TestEqual(TEXT("data untouched"), Data.History.GetLatestFrame(), (int32)INDEX_NONE);
	TestEqual(TEXT("the profile is a usable one even with no source"), Source.GetProfile().GetNumButtons() >= 1, true);

	TArray<FGSTN_InputTargetCandidate> Players;
	FGSTN_LocalInputSource::ListPlayers(Players);
	AddInfo(FString::Printf(TEXT("%d local player(s) listed"), Players.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugWidgetTest, "GSTNInput.Debug.InputDisplay.WidgetFollowsSourceAndScript", GSTN_INPUTDEBUG_TEST_FLAGS)
bool FGSTN_InputDebugWidgetTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Skipped: this run has no Slate application"));
		return true;
	}
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// A source that hands out a history: the widget lays it out on Refresh and keeps the layout while the data does not change
	FGSTN_InputDisplayData Live;
	if (!Play(*this, TEXT("0:4t 3:+A"), *Settings, Live))
	{
		return false;
	}
	FGSTN_InputDisplayProfile Profile;
	Profile.ButtonTags = ButtonTags();
	bool bHasPawn = true;
	int32 NumFetches = 0;
	const TSharedRef<SGSTN_InputDisplay> Widget = SNew(SGSTN_InputDisplay)
		.NumFrames(30)
		.bShowScriptBox(true)
		.Profile(Profile)
		.Fetch(FGSTN_InputDisplayFetch::CreateLambda([&](FGSTN_InputDisplayData& OutData)
		{
			++NumFetches;
			OutData = Live;
			return bHasPawn;
		}));

	Widget->Refresh();
	TestEqual(TEXT("live: columns"), Widget->GetLayout().NumColumns, 4);
	TestEqual(TEXT("live: the profile's six rows"), Widget->GetLayout().Buttons.Num(), 6);
	TestFalse(TEXT("not scripted"), Widget->IsScripted());

	bHasPawn = false;
	Widget->Refresh();
	TestEqual(TEXT("the pawn is gone: nothing shown"), Widget->GetLayout().NumColumns, 0);

	// Scripted: the widget plays the notation and shows that history instead of the live one. Without a command set nothing resolves
	bHasPawn = true;
	TestTrue(TEXT("a valid notation is taken"), Widget->SetScriptedNotation(TEXT("5:+A")));
	TestTrue(TEXT("scripted"), Widget->IsScripted());
	TestFalse(TEXT("no command set, no command"), Widget->GetScriptResult().Command.IsValid());
	TestEqual(TEXT("scripted columns"), Widget->GetLayout().NumColumns, 6);
	TestTrue(TEXT("the press is on the last frame, not greyed"), Widget->GetLayout().Buttons[0][5].Kind == EGSTN_InputCellKind::Press && !Widget->GetLayout().Buttons[0][5].bConsumed);

	const int32 FetchesWhileScripted = NumFetches;
	Widget->Refresh();
	TestEqual(TEXT("a scripted strip does not read the pawn"), NumFetches, FetchesWhileScripted);

	TestFalse(TEXT("a typo is refused"), Widget->SetScriptedNotation(TEXT("banana")));
	TestTrue(TEXT("and keeps the strip"), Widget->IsScripted() && Widget->GetLayout().NumColumns == 6);

	TestTrue(TEXT("clearing goes back to live"), Widget->SetScriptedNotation(FString()));
	TestFalse(TEXT("live again"), Widget->IsScripted());
	TestEqual(TEXT("live columns"), Widget->GetLayout().NumColumns, 4);

	// The interface of the module hands out the display too
	if (IGSTN_InputDebugModule* Module = IGSTN_InputDebugModule::Get())
	{
		TestTrue(TEXT("the module creates the input display"), Module->CreateInputDisplay().IsValid());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
