#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Tests/GSTN_InputDebugTestSupport.h"
#include "Tests/GSTN_VisualSnapshot.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SGSTN_InputDisplay.h"
#include "Widgets/SOverlay.h"

#if WITH_DEV_AUTOMATION_TESTS

// Offscreen snapshots of the input display (GSTN.Visual.InputDisplay.*), so a change to the look can be inspected without a play session. They
// write PNGs to Saved/Notes/ui-snapshots/ and assert nothing about the look: see Tests/GSTN_VisualSnapshot.h for the helper and how to run them.

using namespace GSTNInputDisplayTest;

namespace GSTNVisualInput
{
	/** The history of the sample notation of the display's tests, plus a diagonal sweep and a release: every cell kind, flicks and facing */
	const TCHAR* const SampleNotation = TEXT("0:face=90 0:4t 6:6t! 7:+A 12:2 14:3 16:6 16:+B 20:5 24:-A");

	TSharedRef<SGSTN_InputDisplay> MakeDisplay(bool bShowScriptBox)
	{
		FGSTN_InputDisplayProfile Profile;
		Profile.ButtonTags = ButtonTags();
		return SNew(SGSTN_InputDisplay)
			.bShowScriptBox(bShowScriptBox)
			.Profile(Profile);
	}
}

using namespace GSTNVisualInput;

// The strip as the editor tab shows it: on the panel colour, with the script box

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualInputTabTest, "GSTN.Visual.InputDisplay.EditorTab", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualInputTabTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	const TSharedRef<SGSTN_InputDisplay> Display = MakeDisplay(true);
	if (!TestTrue(TEXT("the sample notation plays"), Display->SetScriptedNotation(SampleNotation)))
	{
		return false;
	}
	const TSharedRef<SWidget> Root = SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(4.f)
		[
			Display
		];
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, Root, FIntPoint(1100, 340), TEXT("input-display-tab-1100x340")).IsEmpty());
	return true;
}

// The corner overlay at a small size: the translucent black of the overlay over a stand-in for the game picture

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualInputOverlayTest, "GSTN.Visual.InputDisplay.SmallOverlay", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualInputOverlayTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	const TSharedRef<SGSTN_InputDisplay> Display = MakeDisplay(false);
	if (!TestTrue(TEXT("the sample notation plays"), Display->SetScriptedNotation(SampleNotation)))
	{
		return false;
	}
	// The border of the overlay (Overlay/GSTN_InputDisplayOverlay.cpp), over a flat grey-blue where the game would be
	const TSharedRef<SWidget> Root = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SColorBlock).Color(FLinearColor(0.08f, 0.1f, 0.14f))
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f))
			.Padding(4.f)
			[
				Display
			]
		];
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, Root, FIntPoint(400, 120), TEXT("input-display-overlay-400x120")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
