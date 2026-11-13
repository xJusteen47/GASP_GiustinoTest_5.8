#include "Display/GSTN_InputDisplayLayout.h"
#include "Display/GSTN_InputDisplayMetrics.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Tests/GSTN_InputDebugTestSupport.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SGSTN_InputDisplay.h"

#if WITH_DEV_AUTOMATION_TESTS

// Idle-run compression of the strip, the pause of the display and the style of a consumed press (GSTNInput.Debug.InputDisplay.*). The slots
// and the style are pure; only the pause needs a widget, and it runs without a window.

#define GSTN_INPUTCOMPRESS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

using namespace GSTNInputDisplayTest;
using FMetrics = FGSTN_InputDisplayMetrics;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugCompressSlotsTest, "GSTNInput.Debug.InputDisplay.IdleRunsCompress", GSTN_INPUTCOMPRESS_TEST_FLAGS)
bool FGSTN_InputDebugCompressSlotsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	if (!Play(*this, TEXT("0:4t 10:+A 30:-A"), *Settings, Data))
	{
		return false;
	}
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data);
	constexpr float Width = 600.f;

	// Frame 0 (stick event), 9 idle, the press, 19 idle with A held, the release: five slots
	const TArray<FGSTN_InputDisplaySlot> Slots = FMetrics::ComputeSlots(Layout, Width, 255, true);
	if (!TestEqual(TEXT("five slots"), Slots.Num(), 5))
	{
		return false;
	}
	TestTrue(TEXT("first run is 9 frames"), Slots[1].IsCompressed() && Slots[1].FirstColumn == 1 && Slots[1].NumColumns == 9);
	TestTrue(TEXT("second run is 19 frames"), Slots[3].IsCompressed() && Slots[3].FirstColumn == 11 && Slots[3].NumColumns == 19);
	TestFalse(TEXT("events stay single columns"), Slots[0].IsCompressed() || Slots[2].IsCompressed() || Slots[4].IsCompressed());
	TestEqual(TEXT("the newest slot ends at the right edge"), Slots.Last().X + Slots.Last().Width, Width - FMetrics::RightMargin);
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("slot %d is inside the strip"), Index), Slots[Index].X >= FMetrics::GutterWidth);
		if (Index > 0)
		{
			TestTrue(*FString::Printf(TEXT("slot %d touches the one before without overlap"), Index), FMath::IsNearlyEqual(Slots[Index].X, Slots[Index - 1].X + Slots[Index - 1].Width, 0.01f));
		}
	}

	// Switched off: one slot per frame, as before
	const TArray<FGSTN_InputDisplaySlot> Plain = FMetrics::ComputeSlots(Layout, Width, 90, false);
	TestEqual(TEXT("uncompressed: a slot per frame"), Plain.Num(), Layout.NumColumns);
	TestFalse(TEXT("uncompressed: no run"), Plain.ContainsByPredicate([](const FGSTN_InputDisplaySlot& Slot) { return Slot.IsCompressed(); }));

	// Runs shorter than the minimum are drawn as they are
	FGSTN_InputDisplayData Short;
	Play(*this, TEXT("0:4t 2:+A"), *Settings, Short);
	TestFalse(TEXT("a run of one frame is not compressed"), FMetrics::ComputeSlots(FGSTN_InputDisplayLayout::Build(Short), Width, 255, true).ContainsByPredicate([](const FGSTN_InputDisplaySlot& Slot) { return Slot.IsCompressed(); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugCompressKeepsPressTest, "GSTNInput.Debug.InputDisplay.CompressedStripKeepsOldPresses", GSTN_INPUTCOMPRESS_TEST_FLAGS)
bool FGSTN_InputDebugCompressKeepsPressTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Data;
	if (!Play(*this, TEXT("0:+A 1:-A 150:+B"), *Settings, Data))
	{
		return false;
	}
	FGSTN_InputDisplayLayoutParams Params;
	Params.MaxColumns = 255;
	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Data, Params);
	constexpr float Width = 400.f;

	// A plain strip of this width holds about 20 frames: the tap of frame 0 is long gone. Compressed, the 148 quiet frames are one slot
	const TArray<FGSTN_InputDisplaySlot> Plain = FMetrics::ComputeSlots(Layout, Width, 90, false);
	TestTrue(TEXT("plain: the early press is cut"), Plain[0].FirstColumn > 0);
	const TArray<FGSTN_InputDisplaySlot> Compressed = FMetrics::ComputeSlots(Layout, Width, 255, true);
	TestEqual(TEXT("compressed: the early press is still the first slot"), Compressed[0].FirstColumn, 0);
	TestEqual(TEXT("compressed: the newest slot is the newest frame"), Compressed.Last().GetLastColumn(), Layout.NumColumns - 1);

	// A strip too narrow for everything keeps the newest slots and still fits
	const TArray<FGSTN_InputDisplaySlot> Narrow = FMetrics::ComputeSlots(Layout, 70.f, 255, true);
	TestTrue(TEXT("narrow: at least the newest slot"), Narrow.Num() >= 1 && Narrow.Last().GetLastColumn() == Layout.NumColumns - 1);
	TestTrue(TEXT("narrow: nothing left of the gutter"), Narrow[0].X >= FMetrics::GutterWidth - 0.01f || Narrow.Num() == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugConsumedStyleTest, "GSTNInput.Debug.InputDisplay.ConsumedPressKeepsLetterAndColour", GSTN_INPUTCOMPRESS_TEST_FLAGS)
bool FGSTN_InputDebugConsumedStyleTest::RunTest(const FString& Parameters)
{
	const auto Contrast = [](const FLinearColor& A, const FLinearColor& B)
	{
		const auto Lum = [](const FLinearColor& C) { return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B; };
		return (FMath::Max(Lum(A), Lum(B)) + 0.05f) / (FMath::Min(Lum(A), Lum(B)) + 0.05f);
	};
	for (int32 Button = 0; Button < 12; ++Button)
	{
		const FLinearColor Colour = FMetrics::GetButtonColor(Button);
		const FGSTN_InputPressIconStyle Plain = FMetrics::GetPressIconStyle(Button, false);
		const FGSTN_InputPressIconStyle Used = FMetrics::GetPressIconStyle(Button, true);
		const FString Name = FString::Printf(TEXT("button %d"), Button);

		TestTrue(*(Name + TEXT(": plain press is the button colour with no outline")), Plain.Fill.Equals(Colour) && Plain.Outline.A == 0.f);
		TestTrue(*(Name + TEXT(": consumed fill is the button hue, darker")), Used.Fill.R <= Colour.R && Used.Fill.G <= Colour.G && Used.Fill.B <= Colour.B && Used.Fill.A == 1.f && !Used.Fill.Equals(Colour, 0.01f));
		TestTrue(*(Name + TEXT(": consumed fill is never white or a flat grey")), Used.Fill.R < 0.6f && !(FMath::IsNearlyEqual(Used.Fill.R, Used.Fill.G, 0.02f) && FMath::IsNearlyEqual(Used.Fill.G, Used.Fill.B, 0.02f)));
		TestTrue(*(Name + TEXT(": consumed has the button colour as outline")), Used.Outline.Equals(FLinearColor(Colour.R, Colour.G, Colour.B, 1.f), 0.001f));
		TestTrue(*(Name + TEXT(": the letter is drawn and readable on the consumed fill")), Used.Letter.A == 1.f && Contrast(Used.Letter, Used.Fill) >= 3.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDebugPauseTest, "GSTNInput.Debug.InputDisplay.PauseFreezesTheStrip", GSTN_INPUTCOMPRESS_TEST_FLAGS)
bool FGSTN_InputDebugPauseTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("Skipped: this run has no Slate application"));
		return true;
	}
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	FGSTN_InputDisplayData Live;
	if (!Play(*this, TEXT("0:4t 3:+A"), *Settings, Live))
	{
		return false;
	}
	FGSTN_InputDisplayProfile Profile;
	Profile.ButtonTags = ButtonTags();
	const TSharedRef<SGSTN_InputDisplay> Widget = SNew(SGSTN_InputDisplay)
		.Profile(Profile)
		.Fetch(FGSTN_InputDisplayFetch::CreateLambda([&Live](FGSTN_InputDisplayData& OutData) { OutData = Live; return true; }));

	TestTrue(TEXT("compression is on by default"), Widget->IsCompressIdle());
	Widget->Refresh();
	TestEqual(TEXT("live: columns"), Widget->GetLayout().NumColumns, 4);

	Widget->SetPaused(true);
	Live = FGSTN_InputDisplayData();
	Play(*this, TEXT("0:4t 3:+A 20:+B"), *Settings, Live);
	Widget->Refresh();
	TestEqual(TEXT("paused: the strip keeps what it showed"), Widget->GetLayout().NumColumns, 4);

	Widget->SetPaused(false);
	Widget->Refresh();
	TestEqual(TEXT("resumed: it catches up"), Widget->GetLayout().NumColumns, 21);

	// Without compression the layout is cut to the frames the strip holds
	Widget->SetCompressIdle(false);
	TestFalse(TEXT("compression off"), Widget->IsCompressIdle());
	TestTrue(TEXT("the layout is rebuilt"), Widget->GetLayout().NumColumns > 0 && Widget->GetLayout().NumColumns <= 90);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
