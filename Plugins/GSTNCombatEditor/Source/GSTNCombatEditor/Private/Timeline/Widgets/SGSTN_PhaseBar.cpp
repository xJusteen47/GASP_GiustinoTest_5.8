// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_PhaseBar.h"

#include "Timeline/GSTN_PhaseBarData.h"
#include "Timeline/GSTN_TimelineColors.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** How close to a handle the mouse has to be to grab it */
	constexpr double PhaseHandleGrabPixels = 6.0;
}

void SGSTN_PhaseBar::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	Gesture = InArgs._Gesture;
	FocusTarget = InArgs._FocusTarget;
	check(Model.IsValid() && Gesture.IsValid());
	RepaintHook = MakeUnique<FGSTN_TimelineRepaintHook>(*Model, *this);

	SetToolTipText(TAttribute<FText>::CreateLambda([this]() -> FText
	{
		const FGSTN_CombatTimelineData* Data = Model->GetData();
		return Data ? FGSTN_PhaseBarData::Compute(*Data).FormatSummary(Model->GetTickRate()) : FText::GetEmpty();
	}));
}

FVector2D SGSTN_PhaseBar::ComputeDesiredSize(float) const
{
	return FVector2D(100.0, GSTNTimelineWidgets::PhaseBarHeight);
}

FLinearColor SGSTN_PhaseBar::PhaseColor(EGSTN_PhaseKind Kind)
{
	return GSTNTimelineColors::PhaseFill(Kind);
}

SGSTN_PhaseBar::EHandle SGSTN_PhaseBar::HitTestHandle(const FVector2D& Local) const
{
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	if (Data == nullptr)
	{
		return EHandle::None;
	}
	const FGSTN_TimelineViewTransform& View = Model->ViewTransform;
	if (Local.Y < GSTNTimelineWidgets::PhaseStripHeight)
	{
		return FMath::Abs(Local.X - View.FrameToPixel(Data->FirstActionableFrame)) <= PhaseHandleGrabPixels ? EHandle::FirstActionable : EHandle::None;
	}
	const int32 TailStart = Data->GetTailStartFrame();
	return (TailStart != INDEX_NONE && FMath::Abs(Local.X - View.FrameToPixel(TailStart)) <= PhaseHandleGrabPixels) ? EHandle::TailStart : EHandle::None;
}

int32 SGSTN_PhaseBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace GSTNTimelineWidgets;

	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FGSTN_TimelineViewTransform& View = Model->ViewTransform;

	DrawRect(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, HeaderBackground());
	if (Data == nullptr)
	{
		return LayerId + 1;
	}

	const FGSTN_PhaseBarData Phases = FGSTN_PhaseBarData::Compute(*Data);
	for (const FGSTN_PhaseSegment& Segment : Phases.Segments)
	{
		const double X0 = View.FrameToPixel(Segment.Start);
		const double X1 = View.FrameToPixel(Segment.End);
		if (X1 < 0.0 || X0 > Size.X)
		{
			continue;
		}
		const FLinearColor Fill = PhaseColor(Segment.Kind);
		DrawRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(X0, 1.0), FVector2D(X1 - X0 - 1.0, PhaseStripHeight - 2.0), Fill);
		// The name and the length when the segment is wide enough for both, the length alone when it fits that, nothing below that
		const double Width = X1 - X0;
		if (Width > 28.0)
		{
			const int32 Frames = Segment.End - Segment.Start;
			const FString Label = Width > 96.0 ? FString::Printf(TEXT("%s %d"), GSTNTimelineColors::PhaseName(Segment.Kind), Frames) : FString::Printf(TEXT("%d"), Frames);
			OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.ToPaintGeometry(FVector2f(FMath::Max(Width - 1.0, 0.0), PhaseStripHeight - 2.0), FSlateLayoutTransform(FVector2f(X0, 1.0)))));
			DrawLabel(OutDrawElements, LayerId + 2, AllottedGeometry, FVector2D(FMath::Max(X0, 0.0) + 4.0, 3.0), Label, GSTNTimelineColors::ReadableTextOn(Fill));
			OutDrawElements.PopClip();
		}
	}

	// The FAF marker, grabbed by its flag
	const double FafX = View.FrameToPixel(Data->FirstActionableFrame);
	DrawVerticalLine(OutDrawElements, LayerId + 3, AllottedGeometry, FafX, 0.0, PhaseStripHeight, FafColor(), 2.0);
	DrawRect(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2D(FafX - 4.0, 0.0), FVector2D(8.0, 5.0), FafColor());

	// The cancellable tail: hatched from its start to the end of the timeline, with a handle on the left
	if (Phases.TailStart != INDEX_NONE)
	{
		const double TailX0 = View.FrameToPixel(Phases.TailStart);
		const double TailX1 = View.FrameToPixel(Data->TotalFrames);
		const FVector2D BandPosition(TailX0, PhaseStripHeight + 3.0);
		const FVector2D BandSize(FMath::Max(TailX1 - TailX0, 2.0), Size.Y - PhaseStripHeight - 5.0);
		DrawRect(OutDrawElements, LayerId + 1, AllottedGeometry, BandPosition, BandSize, TailColor() * FLinearColor(1.f, 1.f, 1.f, 0.25f));
		DrawHatch(OutDrawElements, LayerId + 2, AllottedGeometry, BandPosition, BandSize, TailColor() * FLinearColor(1.f, 1.f, 1.f, 0.8f));
		DrawRect(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2D(TailX0 - 2.0, BandPosition.Y), FVector2D(4.0, BandSize.Y), TailColor());
	}

	return LayerId + 4;
}

FReply SGSTN_PhaseBar::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	bool bArmed = false;
	switch (HitTestHandle(Local))
	{
	case EHandle::FirstActionable:
		bArmed = Gesture->BeginFirstActionable(*Model);
		break;
	case EHandle::TailStart:
		bArmed = Gesture->BeginTailStart(*Model);
		break;
	default:
		break;
	}
	FReply Reply = FReply::Handled();
	if (bArmed)
	{
		Reply.CaptureMouse(AsShared());
	}
	return GSTNTimelineWidgets::WithPanelFocus(Reply, FocusTarget);
}

FReply SGSTN_PhaseBar::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture() || !Gesture->IsActive())
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	Gesture->Update(*Model, Local.X, Local.Y);
	return FReply::Handled();
}

FReply SGSTN_PhaseBar::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	Gesture->End(*Model);
	return FReply::Handled().ReleaseMouseCapture();
}

void SGSTN_PhaseBar::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// Nothing to cancel after a normal mouse-up: End already closed the gesture
	Gesture->Cancel(*Model);
}

FReply SGSTN_PhaseBar::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return GSTNTimelineWidgets::HandleWheel(*Model, MyGeometry, MouseEvent);
}

FCursorReply SGSTN_PhaseBar::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	const bool bDragging = HasMouseCapture() && Gesture->IsActive();
	const FVector2D Local = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	return (bDragging || HitTestHandle(Local) != EHandle::None) ? FCursorReply::Cursor(EMouseCursor::ResizeLeftRight) : FCursorReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
