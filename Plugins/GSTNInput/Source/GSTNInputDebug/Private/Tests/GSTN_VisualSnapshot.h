// Test-only helper of the GSTN.Visual.* tests: draws a Slate widget offscreen and saves it as a PNG.
//
// The same file sits in GSTNInputDebug and in GSTNCombatEditor (Private/Tests): the two modules have no edge to each other and the helper
// is short, so it is copied, not shared. Keep the copies identical.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Flags of the snapshot tests. EngineFilter keeps them out of the product filter the build machines run; they are started by name
 * ("Automation RunTests GSTN.Visual") in a run that has a renderer, and write to Saved/Notes/ui-snapshots/. Without a renderer (-nullrhi) they
 * skip with a warning.
 */
#define GSTN_VISUAL_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace GSTNVisualSnapshot
{
	inline FString GetOutputDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Notes"), TEXT("ui-snapshots")));
	}

	/** True when widgets can be drawn here; otherwise adds the warning that says why and the test should return true */
	inline bool CanRender(FAutomationTestBase& Test)
	{
		if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized() || FSlateApplication::Get().GetRenderer() == nullptr)
		{
			Test.AddWarning(TEXT("Skipped: this run has no renderer or no Slate application (started with -nullrhi?). Run it with a RHI, see Tests/GSTN_VisualSnapshot.h."));
			return false;
		}
		return true;
	}

	/** Calls Tick on a widget and everything below it, the way the application would: list views make their rows in Tick, which a virtual window never gets */
	inline void TickTree(SWidget& Widget, double Time, float DeltaTime)
	{
		Widget.Tick(Widget.GetTickSpaceGeometry(), Time, DeltaTime);
		if (FChildren* Children = Widget.GetChildren())
		{
			Children->ForEachWidget([Time, DeltaTime](SWidget& Child) { TickTree(Child, Time, DeltaTime); });
		}
	}

	/**
	 * Draws the widget into a render target of Size and saves it to ui-snapshots/<Name>.png. The widget is drawn several times with a tick
	 * between the draws, so what needs a layout pass or a tick (list rows, a view that fits to the width) is settled; BetweenPasses runs after each
	 * tick with the pass number, for the caller to do what it can only do once the widget has a size (such as framing the timeline).
	 * Returns the path, empty on failure (reported on Test).
	 */
	inline FString Render(FAutomationTestBase& Test, const TSharedRef<SWidget>& Widget, const FIntPoint& Size, const FString& Name,
		TFunctionRef<void(int32 /*Pass*/)> BetweenPasses = [](int32) {}, int32 NumPasses = 8)
	{
		// Linear target with an sRGB format (no gamma correction in the renderer): the pixels read back are the sRGB bytes the editor shows. With
		// gamma correction on, the colours come out lighter (a #242424 panel read back as #5A5A5A)
		FWidgetRenderer Renderer(false);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
		Target->Filter = TF_Bilinear;
		Target->ClearColor = FLinearColor::Transparent;
		Target->SRGB = true;
		Target->InitCustomFormat(Size.X, Size.Y, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
		if (!Test.TestNotNull(TEXT("a render target was created"), Target.Get()))
		{
			return FString();
		}

		double Time = FPlatformTime::Seconds();
		for (int32 Pass = 0; Pass < NumPasses; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), Widget, FVector2D(Size.X, Size.Y), 1.f / 60.f);
			FlushRenderingCommands();
			// Vector icons (SVG) are rasterized when the application draws its own windows, which a widget renderer does not trigger: tick the
			// application so a first snapshot of the process has its icons, and give the rasterizer a moment
			FSlateApplication::Get().Tick();
			FPlatformProcess::Sleep(0.03f);
			Time += 1.0 / 60.0;
			TickTree(*Widget, Time, 1.f / 60.f);
			BetweenPasses(Pass);
		}
		Renderer.DrawWidget(Target.Get(), Widget, FVector2D(Size.X, Size.Y), 1.f / 60.f);
		FlushRenderingCommands();

		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
		TArray<FColor> Pixels;
		if (!Test.TestTrue(TEXT("the target could be read back"), Resource != nullptr && Resource->ReadPixels(Pixels)) || Pixels.Num() != Size.X * Size.Y)
		{
			return FString();
		}
		for (FColor& Pixel : Pixels)
		{
			Pixel.A = 255;
		}

		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString Path = FPaths::Combine(GetOutputDirectory(), Name + TEXT(".png"));
		IFileManager::Get().MakeDirectory(*GetOutputDirectory(), true);
		if (!Test.TestTrue(*FString::Printf(TEXT("%s was written"), *Path), FFileHelper::SaveArrayToFile(Png, *Path)))
		{
			return FString();
		}
		Test.AddInfo(FString::Printf(TEXT("Snapshot: %s (%dx%d)"), *Path, Size.X, Size.Y));
		return Path;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
