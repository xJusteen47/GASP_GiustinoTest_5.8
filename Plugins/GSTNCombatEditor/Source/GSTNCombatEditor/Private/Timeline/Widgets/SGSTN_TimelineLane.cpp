// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineLane.h"

#include "HAL/PlatformTime.h"
#include "Settings/GSTN_CombatEditorSettings.h"

void SGSTN_TimelineLane::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	Gesture = InArgs._Gesture;
	PaintStats = InArgs._PaintStats;
	FocusTarget = InArgs._FocusTarget;
	TrackIndex = InArgs._TrackIndex;
	check(Model.IsValid() && Gesture.IsValid());
	RepaintHook = MakeUnique<FGSTN_TimelineRepaintHook>(*Model, *this);
}

FVector2D SGSTN_TimelineLane::ComputeDesiredSize(float) const
{
	return FVector2D(100.0, FGSTN_TimelineLayout::RowHeight);
}

FVector2D SGSTN_TimelineLane::ToContent(const FGeometry& Geometry, const FPointerEvent& MouseEvent) const
{
	const FVector2D Local = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	return FVector2D(Local.X, Local.Y + TrackIndex * FGSTN_TimelineLayout::RowHeight);
}

int32 SGSTN_TimelineLane::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	using namespace GSTNTimelineWidgets;

	const double PaintStart = FPlatformTime::Seconds();
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FGSTN_TimelineViewTransform& View = Model->ViewTransform;

	DrawRect(OutDrawElements, LayerId, AllottedGeometry, FVector2D::ZeroVector, Size, LaneBackground((TrackIndex & 1) != 0));
	if (Data == nullptr)
	{
		return LayerId + 1;
	}

	// Past the last frame, then the grid: a line per label step, and per minor step once there is room for it
	const int32 Total = Data->TotalFrames;
	if (Total > 0)
	{
		const double TotalX = FMath::Max(View.FrameToPixel(Total), 0.0);
		DrawRect(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2D(TotalX, 0.0), FVector2D(Size.X - TotalX, Size.Y), BeyondEndShade());
	}
	const FGSTN_RulerTicks Ticks = FGSTN_TimelineLayout::ComputeRulerTicks(View, Size.X);
	const bool bMinorLines = Ticks.MinorStep * View.PixelsPerFrame >= 8.0;
	const int32 Step = bMinorLines ? Ticks.MinorStep : Ticks.LabelStep;
	const int32 FirstLine = FMath::FloorToInt32(static_cast<double>(Ticks.FirstFrame) / Step) * Step;
	for (int32 Frame = FirstLine; Frame <= Ticks.LastFrame; Frame += Step)
	{
		const double X = View.FrameToPixel(Frame);
		if (X >= 0.0 && X <= Size.X)
		{
			DrawVerticalLine(OutDrawElements, LayerId + 2, AllottedGeometry, X, 0.0, Size.Y, (Frame % Ticks.LabelStep) == 0 ? GridMajor() : GridMinor());
		}
	}
	DrawVerticalLine(OutDrawElements, LayerId + 3, AllottedGeometry, View.FrameToPixel(Data->FirstActionableFrame), 0.0, Size.Y, FafColor() * FLinearColor(1.f, 1.f, 1.f, 0.5f));

	// The track
	const FGSTN_TimelineTrack* Track = Model->GetTrack(TrackIndex);
	const UScriptStruct* TrackStruct = Data->Tracks.IsValidIndex(TrackIndex) ? Data->Tracks[TrackIndex].GetScriptStruct() : nullptr;
	if (Track != nullptr && TrackStruct != nullptr)
	{
		const FLinearColor TrackColor = UGSTN_CombatEditorSettings::Get().GetTrackColor(TrackStruct->GetFName());
		const bool bWindowSelected = Model->GetSelection().ContainsByPredicate([this](const FGSTN_Selection& Item) { return Item.TrackIndex == TrackIndex && !Item.IsKey(); });
		const bool bPoint = FGSTN_TimelineEditorModel::IsPointTrack(*Track);
		int32 Start = 0;
		int32 End = 0;
		FGSTN_TimelineEditorModel::GetTrackSpan(*Track, Start, End);

		// Outside [0, TotalFrames) or empty: the validator reports it, the lane shows it hatched red (a timeline with no length has no range to be outside of)
		const bool bInvalid = Start < 0 || End <= Start || (Total > 0 && End > Total);
		const double CentreY = Size.Y * 0.5;

		if (bPoint)
		{
			const FVector2D Centre(View.FrameToPixel(static_cast<double>(Start) + 0.5), CentreY);
			if (bWindowSelected)
			{
				DrawDiamond(OutDrawElements, LayerId + 4, AllottedGeometry, Centre, 9.0, SelectionOutline());
			}
			DrawDiamond(OutDrawElements, LayerId + 5, AllottedGeometry, Centre, 7.0, bInvalid ? InvalidColor() : TrackColor);
		}
		else
		{
			const double X0 = FMath::Max(View.FrameToPixel(Start), -4.0);
			const double X1 = FMath::Min(FMath::Max(View.FrameToPixel(End), View.FrameToPixel(Start) + 2.0), Size.X + 4.0);
			if (X1 > X0)
			{
				const FVector2D BarPosition(X0, 4.0);
				const FVector2D BarSize(X1 - X0, Size.Y - 8.0);
				if (bWindowSelected)
				{
					DrawRect(OutDrawElements, LayerId + 4, AllottedGeometry, BarPosition - FVector2D(2.0, 2.0), BarSize + FVector2D(4.0, 4.0), SelectionOutline(), true);
				}
				DrawRect(OutDrawElements, LayerId + 5, AllottedGeometry, BarPosition, BarSize, bInvalid ? TrackColor * FLinearColor(0.5f, 0.5f, 0.5f, 1.f) : TrackColor, true);
				if (bInvalid)
				{
					DrawHatch(OutDrawElements, LayerId + 6, AllottedGeometry, BarPosition, BarSize, InvalidColor());
				}
				else if (BarSize.X > 30.0)
				{
					OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.ToPaintGeometry(FVector2f(BarSize), FSlateLayoutTransform(FVector2f(BarPosition)))));
					DrawLabel(OutDrawElements, LayerId + 6, AllottedGeometry, FVector2D(BarPosition.X + 4.0, 6.0), TrackStruct->GetDisplayNameText().ToString(), FLinearColor(0.02f, 0.02f, 0.02f, 0.9f));
					OutDrawElements.PopClip();
				}
			}
		}

		// Keys on top of the bar. The one on the playhead frame is bigger
		const int32 KeyCount = FGSTN_TimelineLayout::GetKeyCount(*Data, TrackIndex);
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			const int32 KeyFrame = FGSTN_TimelineLayout::GetKeyFrame(*Data, TrackIndex, KeyIndex);
			const double KeyX = View.FrameToPixel(static_cast<double>(KeyFrame) + 0.5);
			if (KeyX < -10.0 || KeyX > Size.X + 10.0)
			{
				continue;
			}
			const double HalfSize = KeyFrame == Model->GetPlayhead() ? 7.0 : 5.0;
			if (Model->IsSelected(FGSTN_Selection::Key(TrackIndex, KeyIndex)))
			{
				DrawDiamond(OutDrawElements, LayerId + 7, AllottedGeometry, FVector2D(KeyX, CentreY), HalfSize + 2.0, SelectionOutline());
			}
			DrawDiamond(OutDrawElements, LayerId + 8, AllottedGeometry, FVector2D(KeyX, CentreY), HalfSize, FLinearColor(0.98f, 0.9f, 0.55f));
		}
	}

	// Playhead
	const double PlayX = View.FrameToPixel(Model->GetPlayhead());
	DrawRect(OutDrawElements, LayerId + 9, AllottedGeometry, FVector2D(PlayX, 0.0), FVector2D(FMath::Max(View.PixelsPerFrame, 2.0), Size.Y), PlayheadColor() * FLinearColor(1.f, 1.f, 1.f, 0.18f));
	DrawVerticalLine(OutDrawElements, LayerId + 9, AllottedGeometry, PlayX + 1.0, 0.0, Size.Y, PlayheadColor(), 2.0);

	// The rectangle of a box select that started in any lane, cut to this row
	FVector2D BoxMin;
	FVector2D BoxMax;
	if (Gesture->GetBox(BoxMin, BoxMax))
	{
		const double RowTop = TrackIndex * FGSTN_TimelineLayout::RowHeight;
		const double Top = FMath::Max(BoxMin.Y - RowTop, 0.0);
		const double Bottom = FMath::Min(BoxMax.Y - RowTop, Size.Y);
		if (Bottom > Top)
		{
			DrawRect(OutDrawElements, LayerId + 10, AllottedGeometry, FVector2D(BoxMin.X, Top), FVector2D(BoxMax.X - BoxMin.X, Bottom - Top), BoxSelectFill());
			DrawVerticalLine(OutDrawElements, LayerId + 10, AllottedGeometry, BoxMin.X, Top, Bottom, BoxSelectLine());
			DrawVerticalLine(OutDrawElements, LayerId + 10, AllottedGeometry, BoxMax.X, Top, Bottom, BoxSelectLine());
			if (BoxMin.Y >= RowTop)
			{
				DrawRect(OutDrawElements, LayerId + 10, AllottedGeometry, FVector2D(BoxMin.X, Top), FVector2D(BoxMax.X - BoxMin.X, 1.0), BoxSelectLine());
			}
			if (BoxMax.Y <= RowTop + Size.Y)
			{
				DrawRect(OutDrawElements, LayerId + 10, AllottedGeometry, FVector2D(BoxMin.X, Bottom - 1.0), FVector2D(BoxMax.X - BoxMin.X, 1.0), BoxSelectLine());
			}
		}
	}

	if (PaintStats.IsValid())
	{
		PaintStats->AccumulatedSeconds += FPlatformTime::Seconds() - PaintStart;
	}
	return LayerId + 11;
}

FReply SGSTN_TimelineLane::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	if (Data == nullptr)
	{
		return FReply::Handled();
	}

	const FVector2D Content = ToContent(MyGeometry, MouseEvent);
	const FGSTN_LaneHit Hit = FGSTN_TimelineLayout::HitTestTrack(*Data, TrackIndex, Model->ViewTransform, Content.X);
	if (Hit.IsHit())
	{
		Gesture->BeginOnHit(*Model, Hit, Content.X, MouseEvent.IsShiftDown(), MouseEvent.IsControlDown());
	}
	else
	{
		Gesture->BeginBoxSelect(*Model, Content.X, Content.Y, MouseEvent.IsShiftDown());
	}

	FReply Reply = FReply::Handled();
	if (Gesture->IsActive())
	{
		Reply.CaptureMouse(AsShared());
	}
	return GSTNTimelineWidgets::WithPanelFocus(Reply, FocusTarget);
}

FReply SGSTN_TimelineLane::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture() || !Gesture->IsActive())
	{
		return FReply::Unhandled();
	}
	const FVector2D Content = ToContent(MyGeometry, MouseEvent);
	Gesture->Update(*Model, Content.X, Content.Y);
	if (Gesture->GetKind() == EGSTN_GestureKind::BoxSelect)
	{
		// The rectangle grew but the selection may not have changed: every lane it crosses has to repaint
		Model->NotifyViewChanged();
	}
	return FReply::Handled();
}

FReply SGSTN_TimelineLane::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	const bool bWasBox = Gesture->GetKind() == EGSTN_GestureKind::BoxSelect;
	Gesture->End(*Model);
	if (bWasBox)
	{
		Model->NotifyViewChanged();
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SGSTN_TimelineLane::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const FVector2D Content = ToContent(MyGeometry, MouseEvent);
	if (Data != nullptr && !FGSTN_TimelineLayout::HitTestTrack(*Data, TrackIndex, Model->ViewTransform, Content.X).IsHit())
	{
		FGSTN_TimelineActions::AddAtFrame(*Model, TrackIndex, Model->ViewTransform.PixelToFrameIndex(Content.X));
	}
	return GSTNTimelineWidgets::WithPanelFocus(FReply::Handled(), FocusTarget);
}

FReply SGSTN_TimelineLane::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return GSTNTimelineWidgets::HandleWheel(*Model, MyGeometry, MouseEvent);
}

void SGSTN_TimelineLane::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// After a normal mouse-up the gesture is already closed and this does nothing
	Gesture->Cancel(*Model);
}

FCursorReply SGSTN_TimelineLane::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (HasMouseCapture() && Gesture->IsActive())
	{
		const EGSTN_GestureKind Kind = Gesture->GetKind();
		return (Kind == EGSTN_GestureKind::ResizeStart || Kind == EGSTN_GestureKind::ResizeEnd) ? FCursorReply::Cursor(EMouseCursor::ResizeLeftRight) : FCursorReply::Unhandled();
	}
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	if (Data == nullptr)
	{
		return FCursorReply::Unhandled();
	}
	const FVector2D Local = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	const EGSTN_LaneHitKind Kind = FGSTN_TimelineLayout::HitTestTrack(*Data, TrackIndex, Model->ViewTransform, Local.X).Kind;
	return (Kind == EGSTN_LaneHitKind::StartEdge || Kind == EGSTN_LaneHitKind::EndEdge) ? FCursorReply::Cursor(EMouseCursor::ResizeLeftRight) : FCursorReply::Unhandled();
}
