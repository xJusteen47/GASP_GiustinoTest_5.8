// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Overlay/GSTN_InputDisplayOverlay.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Sources/GSTN_LocalInputSource.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SGSTN_InputDisplay.h"
#include "Widgets/SNullWidget.h"

namespace
{
	TAutoConsoleVariable<int32> CVarInputDisplay(TEXT("gstn.Input.Debug.InputDisplay"), 0,
		TEXT("Shows the input history of the local player in the lower right corner of every game or PIE viewport: buttons, stick, frame gaps, commands.\n")
		TEXT("0 = off (default), 1 = on. Development builds only. The older name gstn.Combat.Debug.InputDisplay sets this variable too (GSTNCombatDebug)."),
		ECVF_Default);

	/** The widest the overlay gets: about 40 columns of the strip's minimum width. A narrower viewport gets what fits */
	constexpr float OverlayMaxWidth = 780.f;
	constexpr float OverlayMinWidth = 200.f;
	constexpr float OverlayMargin = 16.f;
	constexpr int32 OverlayZOrder = 100;
}

void FGSTN_InputDisplayOverlay::Start()
{
	// Five times a second is enough to notice the variable; the widgets tick themselves once they exist
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FGSTN_InputDisplayOverlay::Update), 0.2f);
}

void FGSTN_InputDisplayOverlay::Stop()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	RemoveAll();
}

void FGSTN_InputDisplayOverlay::RemoveAll()
{
	for (const FEntry& Entry : Entries)
	{
		if (UGameViewportClient* Viewport = Entry.Viewport.Get(); Viewport && Entry.Widget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(Entry.Widget.ToSharedRef());
		}
	}
	Entries.Reset();
}

bool FGSTN_InputDisplayOverlay::Update(float DeltaTime)
{
	if (CVarInputDisplay.GetValueOnGameThread() == 0)
	{
		if (!Entries.IsEmpty())
		{
			RemoveAll();
		}
		return true;
	}
	if (!GEngine)
	{
		return true;
	}

	// A viewport that went away (PIE ended) takes its widget with it
	Entries.RemoveAll([](const FEntry& Entry) { return !Entry.Viewport.IsValid(); });

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UGameViewportClient* Viewport = Context.GameViewport;
		if (!Viewport || !Context.World() || (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game))
		{
			continue;
		}
		if (Entries.ContainsByPredicate([Viewport](const FEntry& Entry) { return Entry.Viewport.Get() == Viewport; }))
		{
			continue;
		}

		const TSharedRef<FGSTN_LocalInputSource> Source = MakeShared<FGSTN_LocalInputSource>(Context.World());
		const TWeakObjectPtr<UGameViewportClient> WeakViewport = Viewport;
		const TSharedRef<SWidget> Widget = SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(OverlayMargin)
			[
				SNew(SBox)
				// As wide as the viewport leaves room for, so a small PIE window does not clip the strip off its right edge
				.WidthOverride_Lambda([WeakViewport]() -> FOptionalSize
				{
					FVector2D ViewportSize(OverlayMaxWidth, 0.0);
					if (const UGameViewportClient* Pinned = WeakViewport.Get())
					{
						Pinned->GetViewportSize(ViewportSize);
					}
					return FOptionalSize(FMath::Clamp(static_cast<float>(ViewportSize.X) - 2.f * OverlayMargin, OverlayMinWidth, OverlayMaxWidth));
				})
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f))
					.Padding(4.f)
					[
						SNew(SGSTN_InputDisplay)
						.LocalSource(Source)
					]
				]
			];
		Viewport->AddViewportWidgetContent(Widget, OverlayZOrder);
		Entries.Add({ Viewport, Widget });
	}
	return true;
}
