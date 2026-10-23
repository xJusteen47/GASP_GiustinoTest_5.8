// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineRuler.h"

#include "Timeline/GSTN_TimelineLayout.h"

void SGSTN_TimelineRuler::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	PaintStats = InArgs._PaintStats;
	FocusTarget = InArgs._FocusTarget;
	check(Model.IsValid());
	RepaintHook = MakeUnique<FGSTN_TimelineRepaintHook>(*Model, *this);
}

FVector2D SGSTN_TimelineRuler::ComputeDesiredSize(float) const
{
	return FVector2D(100.0, GSTNTimelineWidgets::RulerHeight);
}

int32 SGSTN_TimelineRuler::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace GSTNTimelineWidgets;

	if (PaintStats.IsValid())
	{
		PaintStats->BeginRepaint();
	}

	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FGSTN_TimelineViewTransform& View = Model->ViewTransform;
	const float TickRate = Model->GetTickRate();

	DrawRect(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, FLinearColor(0.07f, 0.07f, 0.075f));

	// Past the last frame of the timeline
	const int32 Total = Data ? Data->TotalFrames : 0;
	if (Total > 0)
	{
		const double TotalX = FMath::Max(View.FrameToPixel(Total), 0.0);
		DrawRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(TotalX, 0.0), FVector2D(Size.X - TotalX, Size.Y), BeyondEndShade());
	}

	// Ticks and labels
	const FGSTN_RulerTicks Ticks = FGSTN_TimelineLayout::ComputeRulerTicks(View, Size.X);
	const int32 FirstTick = FMath::FloorToInt32(static_cast<double>(Ticks.FirstFrame) / Ticks.MinorStep) * Ticks.MinorStep;
	for (int32 Frame = FirstTick; Frame <= Ticks.LastFrame; Frame += Ticks.MinorStep)
	{
		const double X = View.FrameToPixel(Frame);
		if (X < -1.0 || X > Size.X + 1.0)
		{
			continue;
		}
		const bool bLabelled = (Frame % Ticks.LabelStep) == 0;
		const double TickHeight = bLabelled ? 12.0 : 6.0;
		DrawVerticalLine(OutDrawElements, LayerId + 2, AllottedGeometry, X, Size.Y - TickHeight, Size.Y, bLabelled ? GridMajor() * 4.f : GridMajor() * 2.f);
		if (bLabelled && Frame >= 0)
		{
			DrawLabel(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2D(X + 3.0, 1.0), FString::FromInt(Frame), TextColor());
			DrawLabel(OutDrawElements, LayerId + 3, AllottedGeometry, FVector2D(X + 3.0, 12.0), FGSTN_TimelineLayout::FormatMilliseconds(Frame, TickRate) + TEXT(" ms"), TextColorDim());
		}
	}

	// FAF and Total markers
	if (Data != nullptr)
	{
		const double FafX = View.FrameToPixel(Data->FirstActionableFrame);
		DrawVerticalLine(OutDrawElements, LayerId + 4, AllottedGeometry, FafX, 0.0, Size.Y, FafColor(), 2.0);
		if (Total > 0)
		{
			DrawVerticalLine(OutDrawElements, LayerId + 4, AllottedGeometry, View.FrameToPixel(Total), 0.0, Size.Y, TotalColor(), 2.0);
		}
	}

	// Playhead: the cell of the current frame and its leading edge
	const double PlayX = View.FrameToPixel(Model->GetPlayhead());
	DrawRect(OutDrawElements, LayerId + 5, AllottedGeometry, FVector2D(PlayX, 0.0), FVector2D(FMath::Max(View.PixelsPerFrame, 2.0), Size.Y), PlayheadColor() * FLinearColor(1.f, 1.f, 1.f, 0.3f));
	DrawVerticalLine(OutDrawElements, LayerId + 5, AllottedGeometry, PlayX + 1.0, 0.0, Size.Y, PlayheadColor(), 2.0);

	return LayerId + 6;
}

void SGSTN_TimelineRuler::ScrubTo(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	const FVector2D Local = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	Model->SetPlayhead(Model->ViewTransform.PixelToFrameIndex(Local.X));
}

FReply SGSTN_TimelineRuler::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	ScrubTo(MyGeometry, MouseEvent);
	return GSTNTimelineWidgets::WithPanelFocus(FReply::Handled().CaptureMouse(AsShared()), FocusTarget);
}

FReply SGSTN_TimelineRuler::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	ScrubTo(MyGeometry, MouseEvent);
	return FReply::Handled();
}

FReply SGSTN_TimelineRuler::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SGSTN_TimelineRuler::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return GSTNTimelineWidgets::HandleWheel(*Model, MyGeometry, MouseEvent);
}
