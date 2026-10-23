// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "Overlay/GSTN_InputDisplayOverlay.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/GSTN_LocalInputSource.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SGSTN_InputDisplay.h"
#include "Widgets/SNullWidget.h"

namespace
{
	TAutoConsoleVariable<int32> CVarInputDisplay(TEXT("gstn.Combat.Debug.InputDisplay"), 0,
		TEXT("Shows the input history of the local player in the lower right corner of every game or PIE viewport: buttons, stick, frame gaps, commands.\n")
		TEXT("0 = off (default), 1 = on. Development builds only."),
		ECVF_Default);

	constexpr float OverlayWidth = 460.f;
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
		const TSharedRef<SWidget> Widget = SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(16.f)
			[
				SNew(SBox)
				.WidthOverride(OverlayWidth)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
					.Padding(4.f)
					[
						SNew(SGSTN_InputDisplay)
						.Fetch(FGSTN_InputDisplayFetch::CreateLambda([Source](FGSTN_InputDisplayData& OutData) { return Source->Fetch(OutData); }))
					]
				]
			];
		Viewport->AddViewportWidgetContent(Widget, OverlayZOrder);
		Entries.Add({ Viewport, Widget });
	}
	return true;
}
