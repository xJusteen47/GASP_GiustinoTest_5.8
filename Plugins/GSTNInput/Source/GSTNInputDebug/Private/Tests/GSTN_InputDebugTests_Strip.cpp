#include "Components/ActorComponent.h"
#include "Display/GSTN_InputDisplayLayout.h"
#include "Display/GSTN_InputDisplayMetrics.h"
#include "GameFramework/Pawn.h"
#include "GSTNInputDebugModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sources/GSTN_InputSourceRegistry.h"
#include "Sources/GSTN_InputTargetSelection.h"
#include "Tests/GSTN_InputDebugTestObjects.h"
#include "Tests/GSTN_InputDebugTestSupport.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The pure parts of the strip: where columns and arrows go, which frame-gap labels fit, which player is followed, and how a source is
// found. The painting itself needs a rendering editor and is checked by hand (README, Tests).

#define GSTN_INPUTSTRIP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

using namespace GSTNInputDisplayTest;
using FStripMetrics = FGSTN_InputDisplayMetrics;

// Module

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugModuleTest, "GSTNInput.Debug.Module.LoadsAndExposesInterface", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputDebugModuleTest::RunTest(const FString& Parameters)
{
	// The test runs inside a module of the plugin, so the module is loaded; the point is that an outsider reaches it through
	// the interface by name, without linking
	TestTrue(TEXT("the module is available"), IGSTN_InputDebugModule::IsAvailable());
	IGSTN_InputDebugModule* Module = IGSTN_InputDebugModule::Get();
	if (TestNotNull(TEXT("the interface is reachable by module name"), Module))
	{
		// Null is the answer without a Slate application: callers treat it like an absent module. Asking must be safe
		Module->CreateInputDisplay();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugModuleExportTest, "GSTNInput.Debug.Module.InterfaceIsExported", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputDebugModuleExportTest::RunTest(const FString& Parameters)
{
	// Get() and IsAvailable() have to link from another module, and so do the registry and the pure display types the combat plugin uses.
	// A test inside this module cannot see a missing export, so it reads the declarations
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNInput"));
	if (!TestTrue(TEXT("the plugin is found"), Plugin.IsValid()))
	{
		return false;
	}
	const auto Expect = [&](const TCHAR* RelativePath, const TCHAR* Declaration)
	{
		FString Header;
		if (TestTrue(*FString::Printf(TEXT("%s can be read"), RelativePath), FFileHelper::LoadFileToString(Header, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/GSTNInputDebug/Public"), RelativePath))))
		{
			TestTrue(*FString::Printf(TEXT("%s carries the module export macro"), Declaration), Header.Contains(Declaration));
		}
	};
	Expect(TEXT("GSTNInputDebugModule.h"), TEXT("class GSTNINPUTDEBUG_API IGSTN_InputDebugModule"));
	Expect(TEXT("Sources/GSTN_InputSourceRegistry.h"), TEXT("class GSTNINPUTDEBUG_API FGSTN_InputSourceRegistry"));
	Expect(TEXT("Display/GSTN_InputDisplayLayout.h"), TEXT("struct GSTNINPUTDEBUG_API FGSTN_InputDisplayLayout"));
	Expect(TEXT("Display/GSTN_InputDisplayScript.h"), TEXT("struct GSTNINPUTDEBUG_API FGSTN_InputDisplayScript"));
	return true;
}

// Arrows and columns

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputStripArrowsTest, "GSTNInput.Debug.InputDisplay.ArrowForEachOfTheNineDirections", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputStripArrowsTest::RunTest(const FString& Parameters)
{
	constexpr float D = 0.70710678f;
	struct FExpectation
	{
		EGSTN_StickDirection Direction;
		FVector2f Vector;
		TCHAR Key;
	};
	// Screen space: x right, y down. Up is up on the screen
	const FExpectation Expected[] =
	{
		{ EGSTN_StickDirection::Neutral,   FVector2f(0.f, 0.f),   TEXT('5') },
		{ EGSTN_StickDirection::Up,        FVector2f(0.f, -1.f),  TEXT('8') },
		{ EGSTN_StickDirection::UpRight,   FVector2f(D, -D),      TEXT('9') },
		{ EGSTN_StickDirection::Right,     FVector2f(1.f, 0.f),   TEXT('6') },
		{ EGSTN_StickDirection::DownRight, FVector2f(D, D),       TEXT('3') },
		{ EGSTN_StickDirection::Down,      FVector2f(0.f, 1.f),   TEXT('2') },
		{ EGSTN_StickDirection::DownLeft,  FVector2f(-D, D),      TEXT('1') },
		{ EGSTN_StickDirection::Left,      FVector2f(-1.f, 0.f),  TEXT('4') },
		{ EGSTN_StickDirection::UpLeft,    FVector2f(-D, -D),     TEXT('7') },
	};
	TestEqual(TEXT("all nine directions are covered"), static_cast<int32>(UE_ARRAY_COUNT(Expected)), GSTN_NumStickDirections);

	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	for (const FExpectation& Item : Expected)
	{
		const FString Name = FGSTN_InputDisplayLayout::GetDirectionName(Item.Direction);
		const FVector2f Arrow = FStripMetrics::GetArrowDirection(Item.Direction);
		TestTrue(*FString::Printf(TEXT("%s: arrow (%.3f, %.3f)"), *Name, Arrow.X, Arrow.Y), Arrow.Equals(Item.Vector, 1e-4f));
		TestTrue(*FString::Printf(TEXT("%s: unit length, or none for neutral"), *Name), FMath::IsNearlyEqual(Arrow.Size(), Item.Direction == EGSTN_StickDirection::Neutral ? 0.f : 1.f, 1e-4f));
		TestTrue(*FString::Printf(TEXT("%s: numpad key"), *Name), FGSTN_InputDisplayLayout::GetNumpadKey(Item.Direction) == Item.Key);

		// The same direction through the layout: a stick pushed that way on the first frame is that arrow on the first column
		FGSTN_InputDisplayData Data;
		const FString Notation = Item.Direction == EGSTN_StickDirection::Neutral ? FString(TEXT("0:5 2:+A")) : FString::Printf(TEXT("0:%ct 2:+A"), Item.Key);
		if (Play(*this, *Notation, *Settings, Data))
		{
			const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);
			if (TestTrue(TEXT("the layout has columns"), Layout.NumColumns > 0))
			{
				TestTrue(*FString::Printf(TEXT("%s: the layout's first column holds this direction"), *Name), Layout.Stick[0].StickDirection == Item.Direction);
			}
		}
	}

	// Eight different arrows, and opposite directions point opposite ways
	TSet<FString> Distinct;
	for (const FExpectation& Item : Expected)
	{
		const FVector2f Arrow = FStripMetrics::GetArrowDirection(Item.Direction);
		Distinct.Add(FString::Printf(TEXT("%.2f,%.2f"), Arrow.X, Arrow.Y));
	}
	TestEqual(TEXT("nine distinct pictures: eight arrows and the dot"), Distinct.Num(), 9);
	TestTrue(TEXT("up and down are opposite"), (FStripMetrics::GetArrowDirection(EGSTN_StickDirection::Up) + FStripMetrics::GetArrowDirection(EGSTN_StickDirection::Down)).IsNearlyZero());
	TestTrue(TEXT("up-left and down-right are opposite"), (FStripMetrics::GetArrowDirection(EGSTN_StickDirection::UpLeft) + FStripMetrics::GetArrowDirection(EGSTN_StickDirection::DownRight)).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputStripNewestRightTest, "GSTNInput.Debug.InputDisplay.NewestInputIsOnTheRight", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputStripNewestRightTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	if (!Play(*this, TEXT("2:+A 6:-A 9:*B 12:+C"), *Settings, Data))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);
	if (!TestEqual(TEXT("columns"), Layout.NumColumns, 13))
	{
		return false;
	}

	// Columns run from the oldest frame to the newest: the events come out in the order they happened, left to right
	TestEqual(TEXT("the last column is the newest frame"), Layout.FirstFrame + Layout.NumColumns - 1, Data.History.GetLatestFrame());
	int32 PreviousColumn = -1;
	for (const FGSTN_InputDisplayEvent& Event : Layout.Events)
	{
		TestTrue(*FString::Printf(TEXT("event %s is right of the one before"), *Event.GetLabel()), Event.Column > PreviousColumn);
		PreviousColumn = Event.Column;
	}
	TestTrue(TEXT("the newest press is in the last column"), Layout.Buttons[2][Layout.NumColumns - 1].Kind == EGSTN_InputCellKind::Press);

	// And the strip puts the last column against the right edge, whatever the width and however short the history
	for (const float Width : { 300.f, 800.f, 1600.f })
	{
		const int32 Visible = FStripMetrics::GetVisibleColumns(Width, 90);
		const float ColumnWidth = FStripMetrics::GetColumnWidth(Width, Visible);
		const float LastRight = FStripMetrics::GetColumnX(Width, ColumnWidth, Layout.NumColumns, Layout.NumColumns - 1) + ColumnWidth;
		TestEqual(*FString::Printf(TEXT("the newest column ends at the right edge of a %.0f px strip"), Width), LastRight, Width, 1e-3f);
		float PreviousX = -FLT_MAX;
		for (int32 Column = 0; Column < Layout.NumColumns; ++Column)
		{
			const float X = FStripMetrics::GetColumnX(Width, ColumnWidth, Layout.NumColumns, Column);
			TestTrue(*FString::Printf(TEXT("column %d is right of column %d at %.0f px"), Column, Column - 1, Width), X > PreviousX);
			PreviousX = X;
		}
	}

	// A longer history than the strip is wide cuts the oldest frames: the first drawn column moves right, the last stays
	TestEqual(TEXT("everything fits: nothing cut"), FStripMetrics::GetFirstVisibleColumn(13, 40), 0);
	TestEqual(TEXT("40 columns fit of 90: the 50 oldest are cut"), FStripMetrics::GetFirstVisibleColumn(90, 40), 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputStripSizesTest, "GSTNInput.Debug.InputDisplay.ColumnsAndRowsAreBigEnoughToRead", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputStripSizesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a column is at least 18 px"), FStripMetrics::MinColumnWidth >= 18.f);
	TestTrue(TEXT("an icon fits its column with a gap on each side"), FStripMetrics::IconSize + 2.f <= FStripMetrics::MinColumnWidth);
	TestTrue(TEXT("an icon fits its row with a gap"), FStripMetrics::IconSize + 2.f <= FStripMetrics::ButtonRowHeight);
	TestTrue(TEXT("the stick row is about 24 px"), FStripMetrics::StickRowHeight >= 24.f);
	TestTrue(TEXT("a button row is about 16 px"), FStripMetrics::ButtonRowHeight >= 16.f);
	TestTrue(TEXT("six buttons make a strip taller than the 50 px of the first version"), FStripMetrics::GetStripHeight(6) > 100.f);

	// Any width: columns never narrower than the minimum, they fill the width exactly, and there is always one column
	for (const float Width : { 0.f, 40.f, 100.f, 250.f, 462.f, 800.f, 1100.f, 1700.f, 4000.f })
	{
		for (const int32 Max : { 1, 30, 90 })
		{
			const int32 Visible = FStripMetrics::GetVisibleColumns(Width, Max);
			TestTrue(*FString::Printf(TEXT("width %.0f max %d: between 1 and the maximum (%d)"), Width, Max, Visible), Visible >= 1 && Visible <= Max);
			const float ColumnWidth = FStripMetrics::GetColumnWidth(Width, Visible);
			TestTrue(*FString::Printf(TEXT("width %.0f max %d: column %.1f px is not below the minimum"), Width, Max, ColumnWidth), ColumnWidth >= FStripMetrics::MinColumnWidth);
			if (Width >= FStripMetrics::GutterWidth + FStripMetrics::MinColumnWidth && Visible < Max)
			{
				// Limited by the width, not by the history: the columns use all of it
				TestEqual(*FString::Printf(TEXT("width %.0f max %d: the columns fill the width"), Width, Max), FStripMetrics::GutterWidth + Visible * ColumnWidth, Width, 1e-2f);
			}
		}
	}
	TestEqual(TEXT("an 800 px strip shows 43 columns"), FStripMetrics::GetVisibleColumns(800.f, 90), 43);

	// Six buttons, six colours: told apart
	TSet<uint32> Colours;
	for (int32 Button = 0; Button < 6; ++Button)
	{
		Colours.Add(FStripMetrics::GetButtonColor(Button).ToFColor(true).DWColor());
	}
	TestEqual(TEXT("six distinct button colours"), Colours.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputStripGapLabelsTest, "GSTNInput.Debug.InputDisplay.GapLabelsNeverOverlap", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputStripGapLabelsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();

	// The notation sample: events at frames 0, 5, 6, 7. Only the 5-frame gap is a number: 1-frame gaps and the first event get none
	{
		FGSTN_InputDisplayData Data;
		if (Play(*this, TEXT("0:face=90 0:4t 6:6t! 7:+A"), *Settings, Data))
		{
			const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);
			const float Width = 800.f;
			const int32 Visible = FStripMetrics::GetVisibleColumns(Width, 90);
			const float ColumnWidth = FStripMetrics::GetColumnWidth(Width, Visible);
			const TArray<FGSTN_InputGapLabel> Labels = FStripMetrics::ChooseGapLabels(Layout, FStripMetrics::GetFirstVisibleColumn(Layout.NumColumns, Visible), Width, ColumnWidth, 6.f);
			if (TestEqual(TEXT("one label: the five frame gap"), Labels.Num(), 1))
			{
				TestEqual(TEXT("it says 5"), Labels[0].Text, FString(TEXT("5")));
				TestTrue(TEXT("it spans the events at columns 0 and 5"), Labels[0].FromColumn == 0 && Labels[0].ToColumn == 5);
				const float FromCentre = FStripMetrics::GetColumnX(Width, ColumnWidth, Layout.NumColumns, 0) + ColumnWidth * 0.5f;
				const float ToCentre = FStripMetrics::GetColumnX(Width, ColumnWidth, Layout.NumColumns, 5) + ColumnWidth * 0.5f;
				TestEqual(TEXT("and is centred between them"), Labels[0].X + Labels[0].Width * 0.5f, (FromCentre + ToCentre) * 0.5f, 1e-2f);
			}
		}
	}

	// A busy history: an event every second frame (the history keeps 16 button events, so that is all there is). At every width no two labels
	// touch and all stay inside the strip
	FGSTN_InputDisplayData Busy;
	FString Notation;
	int32 Frame = 0;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		Notation += FString::Printf(TEXT("%d:%s%c "), Frame, Index % 2 == 0 ? TEXT("+") : TEXT("-"), TEXT('A') + (Index / 2) % 4);
		Frame += 2;
	}
	if (!Play(*this, *Notation, *Settings, Busy))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Busy);
	TestEqual(TEXT("the busy history has all sixteen events the history keeps"), Layout.Events.Num(), 16);

	for (const float Width : { 120.f, 300.f, 462.f, 800.f, 1400.f })
	{
		// A narrow strip packs the labels too: shrink the column below its minimum to prove the rule, not only the usual widths
		for (const float ColumnWidth : { FStripMetrics::MinColumnWidth, 12.f, 6.f, 3.f })
		{
			const int32 Visible = FStripMetrics::GetVisibleColumns(Width, 90);
			const int32 First = FStripMetrics::GetFirstVisibleColumn(Layout.NumColumns, Visible);
			const TArray<FGSTN_InputGapLabel> Labels = FStripMetrics::ChooseGapLabels(Layout, First, Width, ColumnWidth, 6.f);
			float PreviousRight = -FLT_MAX;
			for (const FGSTN_InputGapLabel& Label : Labels)
			{
				const FString Where = FString::Printf(TEXT("width %.0f column %.0f label \"%s\""), Width, ColumnWidth, *Label.Text);
				TestTrue(*FString::Printf(TEXT("%s: does not touch the one before"), *Where), Label.X >= PreviousRight + FStripMetrics::GapLabelSpacing - 1e-3f);
				TestTrue(*FString::Printf(TEXT("%s: starts inside the strip"), *Where), Label.X >= FStripMetrics::GutterWidth - 1e-3f);
				TestTrue(*FString::Printf(TEXT("%s: ends inside the strip"), *Where), Label.X + Label.Width <= FMath::Max(Width, FStripMetrics::GutterWidth + Label.Width) + 1e-3f);
				TestTrue(*FString::Printf(TEXT("%s: only a gap of more than one frame"), *Where), Label.Frames > 1);
				TestTrue(*FString::Printf(TEXT("%s: both events are in view"), *Where), Label.FromColumn >= First && Label.ToColumn >= First && Label.FromColumn < Label.ToColumn);
				PreviousRight = Label.X + Label.Width;
			}
		}
	}

	// At the usual column width every gap longer than a frame has its number
	const float Width = 1400.f;
	const int32 Visible = FStripMetrics::GetVisibleColumns(Width, 90);
	const float ColumnWidth = FStripMetrics::GetColumnWidth(Width, Visible);
	const TArray<FGSTN_InputGapLabel> Labels = FStripMetrics::ChooseGapLabels(Layout, FStripMetrics::GetFirstVisibleColumn(Layout.NumColumns, Visible), Width, ColumnWidth, 6.f);
	TestTrue(TEXT("at the normal column width labels are shown"), Labels.Num() > 10);
	for (const FGSTN_InputGapLabel& Label : Labels)
	{
		TestTrue(TEXT("the text is the gap in frames"), Label.Text == FString::FromInt(Label.Frames));
	}

	// One frame apart: nothing to say
	FGSTN_InputDisplayData Adjacent;
	if (Play(*this, TEXT("0:+A 1:+B 2:+C 3:-A"), *Settings, Adjacent))
	{
		const FGSTN_InputDisplayLayout AdjacentLayout = FGSTN_InputDisplayLayout::Build(Adjacent);
		TestEqual(TEXT("events on neighbouring frames get no number"), FStripMetrics::ChooseGapLabels(AdjacentLayout, 0, 800.f, 18.f, 6.f).Num(), 0);
	}
	return true;
}

// Which player the display follows

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputStripTargetTest, "GSTNInput.Debug.InputDisplay.TargetSelectionFollowsTheFocusedWindow", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputStripTargetTest::RunTest(const FString& Parameters)
{
	const auto Candidate = [](const TCHAR* Key, bool bFocused, bool bHasSource)
	{
		FGSTN_InputTargetCandidate Result;
		Result.Key = Key;
		Result.bFocused = bFocused;
		Result.bHasSource = bHasSource;
		return Result;
	};

	FGSTN_InputTargetSelection Selection;
	TestTrue(TEXT("follows the focus by default"), Selection.IsFollowingFocus());
	TestEqual(TEXT("no player, nothing to show"), Selection.Choose({}), FString());

	// A listen server host (PIE 0) and a client (PIE 1), a player with a pawn each. Nothing focused yet: the first one that can be read
	TArray<FGSTN_InputTargetCandidate> Players = { Candidate(TEXT("0/0"), false, false), Candidate(TEXT("1/0"), false, true) };
	TestEqual(TEXT("nothing focused yet: the first with something to read"), Selection.Choose(Players), FString(TEXT("1/0")));

	// The window of the host takes the focus: the display follows it, the other no longer
	Players[0].bFocused = true;
	TestEqual(TEXT("the focused window is shown"), Selection.Choose(Players), FString(TEXT("0/0")));

	// The focus moves to the editor tab (the display itself, typing in its script box): the last window that had it stays
	Players[0].bFocused = false;
	TestEqual(TEXT("the last focused window stays while the focus is elsewhere"), Selection.Choose(Players), FString(TEXT("0/0")));

	// Then the client window is clicked
	Players[1].bFocused = true;
	TestEqual(TEXT("the client window now"), Selection.Choose(Players), FString(TEXT("1/0")));
	Players[1].bFocused = false;
	TestEqual(TEXT("and it stays"), Selection.Choose(Players), FString(TEXT("1/0")));

	// A pick in the combo wins over the focus
	Selection.SetFixed(TEXT("0/0"));
	Players[1].bFocused = true;
	TestFalse(TEXT("no longer following"), Selection.IsFollowingFocus());
	TestEqual(TEXT("the picked player is shown whatever has the focus"), Selection.Choose(Players), FString(TEXT("0/0")));

	// The picked player is gone (a new session without it): back to the focus, and the pick comes back with the player
	const TArray<FGSTN_InputTargetCandidate> OnlyClient = { Candidate(TEXT("1/0"), true, true) };
	TestEqual(TEXT("a pick that is not in the session falls back to the focus"), Selection.Choose(OnlyClient), FString(TEXT("1/0")));
	TestEqual(TEXT("and the pick is kept"), Selection.GetFixedKey(), FString(TEXT("0/0")));
	TestEqual(TEXT("it returns with the player"), Selection.Choose(Players), FString(TEXT("0/0")));

	Selection.SetFollowFocus();
	TestEqual(TEXT("following again: the focused window"), Selection.Choose(Players), FString(TEXT("1/0")));
	return true;
}

// How a source is found

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputStripRegistryTest, "GSTNInput.Debug.InputDisplay.SourceRegistryFindsProvidersAndFinders", GSTN_INPUTSTRIP_TEST_FLAGS)
bool FGSTN_InputStripRegistryTest::RunTest(const FString& Parameters)
{
	FGSTN_InputSourceRegistry& Registry = FGSTN_InputSourceRegistry::Get();
	const FName FinderA(TEXT("TestFinderA"));
	const FName FinderB(TEXT("TestFinderB"));
	const FName ProfileKey(TEXT("TestProfile"));

	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	FGSTN_InputHistory History;
	const int32 NumFindersBefore = Registry.GetNumFinders();

	// Nothing reads a bare pawn
	TestFalse(TEXT("a pawn nothing knows has no source"), Registry.FindSource(*Pawn).IsValid());

	// A finder, by key
	int32 FinderACalls = 0;
	Registry.RegisterFinder(FinderA, [&](APawn& Asked) -> TSharedPtr<IGSTN_InputHistorySource>
	{
		++FinderACalls;
		return MakeShared<FGSTN_TestHistorySource>(&Asked, &History);
	});
	TestEqual(TEXT("registered"), Registry.GetNumFinders(), NumFindersBefore + 1);
	TSharedPtr<IGSTN_InputHistorySource> Found = Registry.FindSource(*Pawn);
	TestTrue(TEXT("the finder's source is returned"), Found.IsValid() && Found->GetSourceObject() == Pawn.Get() && Found->GetInputHistoryView() == &History);
	TestEqual(TEXT("the finder was asked once"), FinderACalls, 1);

	// Registering under the same key replaces, not adds
	Registry.RegisterFinder(FinderA, [&](APawn& Asked) -> TSharedPtr<IGSTN_InputHistorySource> { return nullptr; });
	TestEqual(TEXT("same key, replaced"), Registry.GetNumFinders(), NumFindersBefore + 1);
	TestFalse(TEXT("the replacement finds nothing"), Registry.FindSource(*Pawn).IsValid());

	// Finders are asked in the order they were registered; the first source wins
	Registry.RegisterFinder(FinderB, [&](APawn& Asked) -> TSharedPtr<IGSTN_InputHistorySource> { return MakeShared<FGSTN_TestHistorySource>(&Asked, &History); });
	TestTrue(TEXT("the second finder answers when the first does not"), Registry.FindSource(*Pawn).IsValid());

	// A component that implements the provider interface is found with no finder at all, and before the finders
	UGSTN_TestProviderComponent* Provider = NewObject<UGSTN_TestProviderComponent>(Pawn.Get());
	Pawn->AddOwnedComponent(Provider);
	Found = Registry.FindSource(*Pawn);
	if (TestTrue(TEXT("the provider component hands out a source"), Found.IsValid()))
	{
		TestTrue(TEXT("it is the component's, not the finder's"), Found->GetSourceObject() == Provider);
		TestEqual(TEXT("the component created it once"), Provider->NumCreated, 1);
	}
	Pawn->RemoveOwnedComponent(Provider);
	Registry.UnregisterFinder(FinderA);
	Registry.UnregisterFinder(FinderB);
	TestEqual(TEXT("unregistered"), Registry.GetNumFinders(), NumFindersBefore);
	TestFalse(TEXT("and gone"), Registry.FindSource(*Pawn).IsValid());

	// Default profiles: the generic one, six buttons, until a module registers its own
	const int32 NumProfilesBefore = Registry.GetNumDefaultProfiles();
	Registry.RegisterDefaultProfile(ProfileKey, []()
	{
		FGSTN_InputDisplayProfile Profile;
		Profile.ButtonTags = { FGameplayTag(), FGameplayTag() };
		Profile.Label = FText::FromString(TEXT("test profile"));
		return Profile;
	});
	TestEqual(TEXT("registered"), Registry.GetNumDefaultProfiles(), NumProfilesBefore + 1);
	const FGSTN_InputDisplayProfile Registered = Registry.GetDefaultProfile();
	// An earlier registered profile (the combat one) wins when it names buttons; this one is the answer only when it is first
	TestTrue(TEXT("the default profile names buttons"), Registered.GetNumButtons() >= 1);
	Registry.UnregisterDefaultProfile(ProfileKey);
	TestEqual(TEXT("unregistered"), Registry.GetNumDefaultProfiles(), NumProfilesBefore);
	TestEqual(TEXT("an empty profile counts six buttons"), FGSTN_InputDisplayProfile().GetNumButtons(), 6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
