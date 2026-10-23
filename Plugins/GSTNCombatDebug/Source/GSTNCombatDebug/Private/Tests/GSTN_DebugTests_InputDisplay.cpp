#include "Framework/Application/SlateApplication.h"
#include "GSTNCombatDebugModule.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_StandardSlots.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/GSTN_InputDisplayLayout.h"
#include "Widgets/GSTN_InputDisplayScript.h"
#include "Widgets/GSTN_LocalInputSource.h"
#include "Widgets/SGSTN_InputDisplay.h"

#if WITH_DEV_AUTOMATION_TESTS

/** Flags of every input display test: pure logic, runs in the editor and in headless commandlets */
#define GSTN_DEBUG_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNInputDisplayTest
{
	/** The six standard buttons in bit order, the letters A to F of the notation */
	TArray<FGameplayTag> ButtonTags()
	{
		return { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag(),
			GSTNCombatButtonTags::Button_Crouch.GetTag(), GSTNCombatButtonTags::Button_Shield.GetTag(), GSTNCombatButtonTags::Button_Jump.GetTag() };
	}

	/** Settings with the recommended combat values written out, so the result does not move with a designer's config (as the Core tests do) */
	TStrongObjectPtr<UGSTN_InputSettings> MakeSettings()
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings(NewObject<UGSTN_InputSettings>(GetTransientPackage()));
		Settings->DefaultBufferFrames = 8;
		Settings->bHoldBufferByDefault = true;
		Settings->MaxHoldBufferFrames = 0;
		Settings->DefaultSimultaneityFrames = 3;
		Settings->SimultaneousFrames = 2;
		Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;
		Settings->bChordGraceByDefault = false;
		Settings->ChordGraceFrames = 3;
		Settings->StickDeadZone = 0.2f;
		Settings->AimZone = 0.5f;
		Settings->FlickFrames = 3;
		Settings->DiagonalHalfAngle = 22.5f;
		Settings->WorldReferenceYaw = 0.f;
		Settings->CommandTiers.Empty();
		Settings->CommandTiers.Add(GSTNCommandTags::Attack, 100);
		Settings->CommandTiers.Add(GSTNCommandTags::Strong, 200);
		Settings->CommandTiers.Add(GSTNCommandTags::Special, 300);
		Settings->CommandTiers.Add(GSTNCommandTags::Combo, 350);
		Settings->CommandTiers.Add(GSTNCommandTags::Super, 400);
		Settings->ExclusiveButtons = { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag() };
		return Settings;
	}

	/** Plays a notation into display data the way the display would get it from a pawn, without resolving a command */
	bool Play(FAutomationTestBase& Test, const TCHAR* Notation, const UGSTN_InputSettings& Settings, FGSTN_InputDisplayData& OutData)
	{
		FGSTN_InputScript Script(ButtonTags());
		FString Error;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(OutData.History, Settings, Script.GetLastFrame());
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);
		return bOk;
	}

	/** Resolves a notation against the standard command set */
	FGSTN_InputDisplayScriptResult ResolveStandard(const FString& Notation, const UGSTN_InputSettings& Settings, const TArray<FGSTN_InputSlotSpec>& Slots)
	{
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		return FGSTN_InputDisplayScript::Resolve(Notation, ButtonTags(), Pointers, Settings);
	}
}

using namespace GSTNInputDisplayTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayLayoutSampleTest, "GSTNCombat.Debug.InputDisplay.LayoutOfSampleHistory", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayLayoutSampleTest::RunTest(const FString& Parameters)
{
	// The sample of the notation: stick left with the character turned 90 degrees, then a flick to the right and the attack button
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	if (!Play(*this, TEXT("0:face=90 0:4t 6:6t! 7:+A"), *Settings, Data))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);

	// One column per frame, frames 0 to 7
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

	// Stick: physical numpad key, facing-relative key as the classifier made it
	TestTrue(TEXT("frame 0 physical key"), Layout.Stick[0].StickKey == TEXT('4'));
	TestTrue(TEXT("frame 0 zone"), Layout.Stick[0].Zone == EGSTN_StickZone::Tilt);
	TestTrue(TEXT("frame 0 facing key is the history's"), Layout.Stick[0].FacingKey == FGSTN_InputDisplayLayout::GetNumpadKey(Data.History.GetStickAt(0).FacingDir));
	TestTrue(TEXT("turned 90 degrees, left on the stick is not left of the character"), Layout.Stick[0].FacingKey != TEXT('4'));
	TestTrue(TEXT("stick held until its next event"), Layout.Stick[3].StickKey == TEXT('4') && !Layout.Stick[3].bEvent);
	TestTrue(TEXT("frame 5 neutral"), Layout.Stick[5].StickKey == TEXT('5') && Layout.Stick[5].bEvent);
	TestTrue(TEXT("frame 6 physical key"), Layout.Stick[6].StickKey == TEXT('6'));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayButtonRowsTest, "GSTNCombat.Debug.InputDisplay.ButtonRows", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayButtonRowsTest::RunTest(const FString& Parameters)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayConsumedTest, "GSTNCombat.Debug.InputDisplay.ConsumedPressGreyed", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayConsumedTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();

	// A pressed on frame 0 and released on 4: the neutral attack comes out on frame 4, which consumes the press
	const FGSTN_InputDisplayScriptResult Result = ResolveStandard(TEXT("0:+A 4:-A"), *Settings, Slots);
	if (!TestTrue(TEXT("the script resolves"), Result.bOk))
	{
		return false;
	}
	TestEqual(TEXT("the command"), Result.Command.ToString(), GSTNCommandTags::Attack_Neutral.GetTag().ToString());

	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Result.Data);
	if (!TestEqual(TEXT("columns"), Layout.NumColumns, 5))
	{
		return false;
	}
	const FGSTN_InputDisplayCell& Press = Layout.Buttons[0][0];
	TestTrue(TEXT("the press is a press"), Press.Kind == EGSTN_InputCellKind::Press);
	TestTrue(TEXT("the press the command used is greyed"), Press.bConsumed);
	TestTrue(TEXT("its hold is greyed"), Layout.Buttons[0][2].Kind == EGSTN_InputCellKind::Hold && Layout.Buttons[0][2].bConsumed);
	TestTrue(TEXT("so is the release"), Layout.Buttons[0][4].Kind == EGSTN_InputCellKind::Release && Layout.Buttons[0][4].bConsumed);

	// The command is a marker on its frame, and the greyed press knows which command used it (the hover text)
	TestEqual(TEXT("one marker"), Layout.Markers.Num(), 1);
	TestEqual(TEXT("marker column"), Layout.ColumnMarker[4], 0);
	TestEqual(TEXT("the other columns have none"), Layout.ColumnMarker[3], (int32)INDEX_NONE);
	if (TestTrue(TEXT("the press names its command"), Layout.Markers.IsValidIndex(Press.MarkerIndex)))
	{
		TestEqual(TEXT("which command"), Layout.Markers[Press.MarkerIndex].Command.ToString(), GSTNCommandTags::Attack_Neutral.GetTag().ToString());
	}

	// The same history before any command used it is not greyed
	FGSTN_InputDisplayData Raw;
	if (Play(*this, TEXT("0:+A 4:-A"), *Settings, Raw))
	{
		const FGSTN_InputDisplayLayout Plain = FGSTN_InputDisplayLayout::Build(Raw);
		TestFalse(TEXT("an unused press is not greyed"), Plain.Buttons[0][0].bConsumed);
		TestEqual(TEXT("and has no marker"), Plain.Markers.Num(), 0);
	}

	// A marker too far after the press is not blamed on it: the press stays greyed, with no command named
	FGSTN_InputDisplayData Far;
	if (Play(*this, TEXT("0:+A"), *Settings, Far))
	{
		Far.History.Consume(Far.History.GetButtonSlot(0));
		Far.Markers.Add({ 30, GSTNCommandTags::Attack_Neutral.GetTag() });
		FGSTN_InputDisplayLayoutParams Params;
		Params.NowFrame = 40;
		const FGSTN_InputDisplayLayout Late = FGSTN_InputDisplayLayout::Build(Far, Params);
		TestTrue(TEXT("a consumed press whose command is far away stays greyed"), Late.Buttons[0][0].bConsumed);
		TestEqual(TEXT("without a command name"), Late.Buttons[0][0].MarkerIndex, (int32)INDEX_NONE);

		Far.Markers[0].Frame = 5;
		const FGSTN_InputDisplayLayout Near = FGSTN_InputDisplayLayout::Build(Far, Params);
		TestEqual(TEXT("a marker within reach names its command"), Near.Buttons[0][0].MarkerIndex, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayFlickTest, "GSTNCombat.Debug.InputDisplay.FlickMarker", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayFlickTest::RunTest(const FString& Parameters)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayScriptedTest, "GSTNCombat.Debug.InputDisplay.ScriptedHistoryResolvesCommand", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayScriptedTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();

	const auto Expect = [&](const TCHAR* Notation, const FNativeGameplayTag& Expected)
	{
		const FGSTN_InputDisplayScriptResult Result = ResolveStandard(Notation, *Settings, Slots);
		TestTrue(*FString::Printf(TEXT("[%s] plays"), Notation), Result.bOk);
		TestEqual(*FString::Printf(TEXT("[%s] command"), Notation), Result.Command.ToString(), Expected.GetTag().ToString());
	};
	Expect(TEXT("5:+A"), GSTNCommandTags::Attack_Neutral);
	Expect(TEXT("0:8t 5:+A"), GSTNCommandTags::Attack_Directional);
	Expect(TEXT("0:+D 5:+A"), GSTNCommandTags::Attack_CrouchNeutral);
	Expect(TEXT("0:+D 0:8t 5:+C"), GSTNCommandTags::Special_CrouchDirectional);

	// The result carries the history, the consumed press and the marker the strip draws
	const FGSTN_InputDisplayScriptResult Result = ResolveStandard(TEXT("5:+A"), *Settings, Slots);
	TestEqual(TEXT("last frame"), Result.LastFrame, 5);
	TestTrue(TEXT("a slot index"), Slots.IsValidIndex(Result.SlotIndex));
	if (TestEqual(TEXT("one marker"), Result.Data.Markers.Num(), 1))
	{
		TestEqual(TEXT("on the last frame"), Result.Data.Markers[0].Frame, 5);
	}
	TestTrue(TEXT("the strip greys the press"), FGSTN_InputDisplayLayout::Build(Result.Data).Buttons[0][5].bConsumed);

	// Input that no command of the set takes: legal, no command, nothing greyed
	const FGSTN_InputDisplayScriptResult Nothing = ResolveStandard(TEXT("0:6t"), *Settings, Slots);
	TestTrue(TEXT("plays"), Nothing.bOk);
	TestFalse(TEXT("no command"), Nothing.Command.IsValid());
	TestEqual(TEXT("no slot"), Nothing.SlotIndex, (int32)INDEX_NONE);
	TestEqual(TEXT("no marker"), Nothing.Data.Markers.Num(), 0);

	// A kit without the command: the same notation resolves to what is left (or to nothing)
	TArray<FGSTN_InputSlotSpec> Reduced = Slots;
	Reduced.RemoveAll([](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == GSTNCommandTags::Attack_Neutral.GetTag(); });
	TestFalse(TEXT("a set without the neutral attack takes the press as nothing"), ResolveStandard(TEXT("5:+A"), *Settings, Reduced).Command.IsValid());

	// A typo is an answer, not a crash or a log error: the syntax errors come back in Error
	const FGSTN_InputDisplayScriptResult Bad = ResolveStandard(TEXT("banana"), *Settings, Slots);
	TestFalse(TEXT("not a notation"), Bad.bOk);
	TestFalse(TEXT("with a reason"), Bad.Error.IsEmpty());
	TestEqual(TEXT("and no data"), Bad.Data.History.GetLatestFrame(), (int32)INDEX_NONE);
	TestFalse(TEXT("empty notation"), ResolveStandard(TEXT("  "), *Settings, Slots).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayPureTest, "GSTNCombat.Debug.InputDisplay.LayoutIsPure", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayPureTest::RunTest(const FString& Parameters)
{
	// The layout is a pure function: read its two files and look at what they include. No Slate, no widget, no world.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNCombatDebug"));
	if (!TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		return false;
	}
	const FString Dir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNCombatDebug/Private/Widgets"));
	for (const TCHAR* File : { TEXT("GSTN_InputDisplayLayout.h"), TEXT("GSTN_InputDisplayLayout.cpp") })
	{
		FString Text;
		if (!TestTrue(FString::Printf(TEXT("%s can be read"), File), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, File))))
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
			for (const TCHAR* Forbidden : { TEXT("Slate"), TEXT("Widgets/S"), TEXT("Framework/"), TEXT("Engine/World"), TEXT("GameFramework"), TEXT("Simulation/"), TEXT("Layout/") })
			{
				TestFalse(FString::Printf(TEXT("%s: \"%s\" does not include %s"), File, *Line.TrimStartAndEnd(), Forbidden), Line.Contains(Forbidden));
			}
		}
		TestTrue(FString::Printf(TEXT("%s has includes to check"), File), NumIncludes > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayLocalSourceTest, "GSTNCombat.Debug.InputDisplay.NoLocalPawnShowsNothing", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayLocalSourceTest::RunTest(const FString& Parameters)
{
	// No play session runs under the automation test: the source says there is nothing, and leaves the data alone
	FGSTN_LocalInputSource Source;
	FGSTN_InputDisplayData Data;
	TestFalse(TEXT("no local combat pawn, no data"), Source.Fetch(Data));
	TestNull(TEXT("no component"), Source.GetComponent());
	TestEqual(TEXT("data untouched"), Data.History.GetLatestFrame(), (int32)INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayWidgetTest, "GSTNCombat.Debug.InputDisplay.WidgetFollowsSourceAndScript", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayWidgetTest::RunTest(const FString& Parameters)
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
	bool bHasPawn = true;
	int32 NumFetches = 0;
	const TSharedRef<SGSTN_InputDisplay> Widget = SNew(SGSTN_InputDisplay)
		.NumFrames(30)
		.bShowScriptBox(true)
		.Fetch(FGSTN_InputDisplayFetch::CreateLambda([&](FGSTN_InputDisplayData& OutData)
		{
			++NumFetches;
			OutData = Live;
			return bHasPawn;
		}));

	Widget->Refresh();
	TestEqual(TEXT("live: columns"), Widget->GetLayout().NumColumns, 4);
	TestFalse(TEXT("not scripted"), Widget->IsScripted());

	bHasPawn = false;
	Widget->Refresh();
	TestEqual(TEXT("the pawn is gone: nothing shown"), Widget->GetLayout().NumColumns, 0);

	// Scripted: the widget resolves against the standard set and shows that history instead of the live one
	bHasPawn = true;
	TestTrue(TEXT("a valid notation is taken"), Widget->SetScriptedNotation(TEXT("5:+A")));
	TestTrue(TEXT("scripted"), Widget->IsScripted());
	TestEqual(TEXT("the command"), Widget->GetScriptResult().Command.ToString(), GSTNCommandTags::Attack_Neutral.GetTag().ToString());
	TestEqual(TEXT("scripted columns"), Widget->GetLayout().NumColumns, 6);
	TestTrue(TEXT("the used press is greyed"), Widget->GetLayout().Buttons[0][5].bConsumed);

	const int32 FetchesWhileScripted = NumFetches;
	Widget->Refresh();
	TestEqual(TEXT("a scripted strip does not read the pawn"), NumFetches, FetchesWhileScripted);

	TestFalse(TEXT("a typo is refused"), Widget->SetScriptedNotation(TEXT("banana")));
	TestTrue(TEXT("and keeps the strip"), Widget->IsScripted() && Widget->GetLayout().NumColumns == 6);

	TestTrue(TEXT("clearing goes back to live"), Widget->SetScriptedNotation(FString()));
	TestFalse(TEXT("live again"), Widget->IsScripted());
	TestEqual(TEXT("live columns"), Widget->GetLayout().NumColumns, 4);

	// The interface of the module hands out a strip too
	if (IGSTN_CombatDebugModule* Module = IGSTN_CombatDebugModule::Get())
	{
		TestTrue(TEXT("the module creates the input display"), Module->CreateInputDisplay().IsValid());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
