// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Widgets/SWidget.h"

/**
 * What the timeline widgets share: sizes, colours and the few drawing helpers. The widgets paint with plain boxes and lines
 * (no child widget per key, combat-m5-editor 5.2), so this is most of what "style" means here.
 */
namespace GSTNTimelineWidgets
{
	/** Width of the track name column, also the offset of the ruler and the phase bar so frames line up with the lanes */
	constexpr float NameColumnWidth = 200.f;

	/** Width of the vertical scrollbar of the rows, mirrored by a spacer beside the ruler and the phase bar */
	constexpr float ScrollbarWidth = 12.f;

	constexpr float RulerHeight = 34.f;
	constexpr float PhaseBarHeight = 30.f;

	/** The strip of the phase bar that holds the phases and the FAF marker; below it is the strip of the tail */
	constexpr float PhaseStripHeight = 18.f;

	inline const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	inline const FSlateBrush* RoundedBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 3.0f);
		return &Brush;
	}

	inline FSlateFontInfo SmallFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", 8);
	}

	// Colours. Plain values: the timeline reads like a dark editor panel in the default theme

	inline FLinearColor LaneBackground(bool bOddRow)
	{
		return bOddRow ? FLinearColor(0.095f, 0.095f, 0.10f) : FLinearColor(0.115f, 0.115f, 0.12f);
	}
	inline FLinearColor BeyondEndShade() { return FLinearColor(0.f, 0.f, 0.f, 0.35f); }
	inline FLinearColor GridMinor() { return FLinearColor(1.f, 1.f, 1.f, 0.05f); }
	inline FLinearColor GridMajor() { return FLinearColor(1.f, 1.f, 1.f, 0.11f); }
	inline FLinearColor PlayheadColor() { return FLinearColor(0.95f, 0.15f, 0.15f); }
	inline FLinearColor FafColor() { return FLinearColor(0.35f, 0.85f, 0.4f); }
	inline FLinearColor TotalColor() { return FLinearColor(0.85f, 0.85f, 0.85f); }
	inline FLinearColor TailColor() { return FLinearColor(0.95f, 0.7f, 0.2f); }
	inline FLinearColor InvalidColor() { return FLinearColor(0.9f, 0.1f, 0.1f); }
	inline FLinearColor SelectionOutline() { return FLinearColor(1.f, 1.f, 1.f); }
	inline FLinearColor BoxSelectFill() { return FLinearColor(0.4f, 0.6f, 1.f, 0.18f); }
	inline FLinearColor BoxSelectLine() { return FLinearColor(0.5f, 0.7f, 1.f, 0.9f); }
	inline FLinearColor TextColor() { return FLinearColor(0.85f, 0.85f, 0.85f); }
	inline FLinearColor TextColorDim() { return FLinearColor(0.6f, 0.6f, 0.6f); }

	/** The wheel over the ruler, the phase bar or a lane: scroll, or zoom around the cursor with Ctrl */
	inline FReply HandleWheel(FGSTN_TimelineEditorModel& Model, const FGeometry& Geometry, const FPointerEvent& MouseEvent)
	{
		const FVector2D Local = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		FGSTN_TimelineLayout::ApplyWheel(Model.ViewTransform, Local.X, MouseEvent.GetWheelDelta(), MouseEvent.IsControlDown(), Geometry.GetLocalSize().X, Model.GetTotalFrames());
		Model.NotifyViewChanged();
		return FReply::Handled();
	}

	/** Gives the keyboard focus to the panel, so its commands see the keys typed after a click on a part of it */
	inline FReply WithPanelFocus(FReply Reply, const TWeakPtr<SWidget>& FocusTarget)
	{
		if (const TSharedPtr<SWidget> Target = FocusTarget.Pin())
		{
			Reply.SetUserFocus(Target.ToSharedRef(), EFocusCause::Mouse);
		}
		return Reply;
	}

	/** A filled rectangle at a position and size in the local space of the geometry */
	inline void DrawRect(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color, bool bRounded = false)
	{
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			bRounded ? RoundedBrush() : WhiteBrush(), ESlateDrawEffect::None, Color);
	}

	/** A one pixel vertical line from y0 to y1 at x (the width is centred on x) */
	inline void DrawVerticalLine(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, double X, double Y0, double Y1, const FLinearColor& Color, double Width = 1.0)
	{
		DrawRect(OutDrawElements, LayerId, Geometry, FVector2D(X - Width * 0.5, Y0), FVector2D(Width, Y1 - Y0), Color);
	}

	/** A square turned 45 degrees, centred at the point, HalfSize from the centre to a corner */
	inline void DrawDiamond(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Centre, double HalfSize, const FLinearColor& Color)
	{
		// A square with the diagonal HalfSize * 2 has the side HalfSize * sqrt(2)
		const double Side = HalfSize * 1.41421356;
		FSlateDrawElement::MakeRotatedBox(OutDrawElements, LayerId,
			Geometry.ToPaintGeometry(FVector2f(Side, Side), FSlateLayoutTransform(FVector2f(Centre.X - Side * 0.5, Centre.Y - Side * 0.5))),
			WhiteBrush(), ESlateDrawEffect::None, UE_PI * 0.25f, TOptional<FVector2f>(FVector2f(Side * 0.5, Side * 0.5)), FSlateDrawElement::RelativeToElement, Color);
	}

	/** Diagonal lines across a rectangle, clipped to it: the hatching of an invalid window and of the tail */
	inline void DrawHatch(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color, double Spacing = 6.0)
	{
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}
		OutDrawElements.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position)))));
		for (double Offset = -Size.Y; Offset < Size.X; Offset += Spacing)
		{
			TArray<FVector2f> Line;
			Line.Add(FVector2f(Position.X + Offset, Position.Y + Size.Y));
			Line.Add(FVector2f(Position.X + Offset + Size.Y, Position.Y));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), MoveTemp(Line), ESlateDrawEffect::None, Color, false, 1.0f);
		}
		OutDrawElements.PopClip();
	}

	/** Text with its top-left corner at the position */
	inline void DrawLabel(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& Geometry, const FVector2D& Position, const FString& Text, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2f(1000.f, 16.f), FSlateLayoutTransform(FVector2f(Position))),
			Text, SmallFont(), ESlateDrawEffect::None, Color);
	}
}

/**
 * Seconds the lanes spent painting, summed per repaint of the panel: the ruler paints first and closes the previous total, the
 * lanes add to the current one. A readout for the acceptance line "usable at 12 tracks x 60 keys" (plan m5 E4), not a profiler.
 */
struct FGSTN_TimelinePaintStats
{
	double AccumulatedSeconds = 0.0;

	/** Paint time of the last complete repaint, in milliseconds */
	double LastRepaintMilliseconds = 0.0;

	void BeginRepaint()
	{
		LastRepaintMilliseconds = AccumulatedSeconds * 1000.0;
		AccumulatedSeconds = 0.0;
	}
};

/**
 * Asks a widget to repaint whenever the model says something it draws changed: the data, the selection, the playhead or the
 * view. Owned by the widget, so it unsubscribes before the widget goes away; the widget holds the model by shared reference and
 * declares the hook after it.
 */
class FGSTN_TimelineRepaintHook
{
public:
	FGSTN_TimelineRepaintHook(FGSTN_TimelineEditorModel& InModel, SWidget& InWidget)
		: Model(InModel), Widget(InWidget)
	{
		DataHandle = Model.OnDataChanged.AddRaw(this, &FGSTN_TimelineRepaintHook::Repaint);
		SelectionHandle = Model.OnSelectionChanged.AddRaw(this, &FGSTN_TimelineRepaintHook::Repaint);
		ViewHandle = Model.OnViewChanged.AddRaw(this, &FGSTN_TimelineRepaintHook::Repaint);
		PlayheadHandle = Model.OnPlayheadChanged.AddRaw(this, &FGSTN_TimelineRepaintHook::RepaintFrame);
	}

	~FGSTN_TimelineRepaintHook()
	{
		Model.OnDataChanged.Remove(DataHandle);
		Model.OnSelectionChanged.Remove(SelectionHandle);
		Model.OnViewChanged.Remove(ViewHandle);
		Model.OnPlayheadChanged.Remove(PlayheadHandle);
	}

	FGSTN_TimelineRepaintHook(const FGSTN_TimelineRepaintHook&) = delete;
	FGSTN_TimelineRepaintHook& operator=(const FGSTN_TimelineRepaintHook&) = delete;

private:
	void Repaint() { Widget.Invalidate(EInvalidateWidgetReason::Paint); }
	void RepaintFrame(int32) { Repaint(); }

	FGSTN_TimelineEditorModel& Model;
	SWidget& Widget;
	FDelegateHandle DataHandle;
	FDelegateHandle SelectionHandle;
	FDelegateHandle ViewHandle;
	FDelegateHandle PlayheadHandle;
};
