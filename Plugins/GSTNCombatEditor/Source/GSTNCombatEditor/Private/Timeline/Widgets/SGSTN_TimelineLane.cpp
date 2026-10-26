// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineLane.h"

#include "HAL/PlatformTime.h"
#include "Timeline/GSTN_TimelineColors.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"

void SGSTN_TimelineLane::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	Gesture = InArgs._Gesture;
	PaintStats = InArgs._PaintStats;
	Findings = InArgs._Findings;
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
		const TSharedRef<const IGSTN_TrackEditor> TrackEditor = FGSTN_TrackEditorRegistry::Get().FindOrGeneric(TrackStruct);
		const FLinearColor TrackColor = TrackEditor->GetTrackColor(*Track);
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
			DrawDiamond(OutDrawElements, LayerId + 5, AllottedGeometry, Centre, 7.0, bInvalid ? InvalidColor() : GSTNTimelineColors::BarOutline(TrackColor));
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
				// A dark fill inside a one pixel outline of the track colour; the text on the fill is white or black by the fill's luminance (GSTN_TimelineColors.h)
				const FLinearColor Fill = GSTNTimelineColors::BarFill(TrackColor) * (bInvalid ? FLinearColor(0.6f, 0.6f, 0.6f, 1.f) : FLinearColor::White);
				DrawRect(OutDrawElements, LayerId + 5, AllottedGeometry, BarPosition, BarSize, GSTNTimelineColors::BarOutline(TrackColor), true);
				DrawRect(OutDrawElements, LayerId + 6, AllottedGeometry, BarPosition + FVector2D(1.0, 1.0), FVector2D(FMath::Max(BarSize.X - 2.0, 0.0), FMath::Max(BarSize.Y - 2.0, 0.0)), Fill, true);
				if (bInvalid)
				{
					DrawHatch(OutDrawElements, LayerId + 7, AllottedGeometry, BarPosition, BarSize, InvalidColor());
				}
				else if (BarSize.X > 30.0)
				{
					// The label goes where no key diamond covers it: left of the first key, else right of the last one, else nowhere (the name
					// column's tooltip has it). A track without keys has the whole bar
					double LabelX0 = BarPosition.X + 4.0;
					double LabelX1 = BarPosition.X + BarSize.X;
					const int32 LabelKeyCount = FGSTN_TimelineLayout::GetKeyCount(*Data, TrackIndex);
					if (LabelKeyCount > 0)
					{
						double FirstKeyX = TNumericLimits<double>::Max();
						double LastKeyX = -TNumericLimits<double>::Max();
						for (int32 KeyIndex = 0; KeyIndex < LabelKeyCount; ++KeyIndex)
						{
							const double LabelKeyX = View.FrameToPixel(static_cast<double>(FGSTN_TimelineLayout::GetKeyFrame(*Data, TrackIndex, KeyIndex)) + 0.5);
							FirstKeyX = FMath::Min(FirstKeyX, LabelKeyX);
							LastKeyX = FMath::Max(LastKeyX, LabelKeyX);
						}
						constexpr double MinLabelWidth = 48.0;
						if (FirstKeyX - 8.0 - LabelX0 >= MinLabelWidth)
						{
							LabelX1 = FirstKeyX - 8.0;
						}
						else if (LabelX1 - (LastKeyX + 8.0) >= MinLabelWidth)
						{
							LabelX0 = LastKeyX + 8.0;
						}
						else
						{
							LabelX1 = LabelX0;
						}
					}
					if (LabelX1 > LabelX0)
					{
						OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.ToPaintGeometry(FVector2f(FVector2D(LabelX1 - LabelX0, BarSize.Y)), FSlateLayoutTransform(FVector2f(FVector2D(LabelX0, BarPosition.Y))))));
						DrawLabel(OutDrawElements, LayerId + 7, AllottedGeometry, FVector2D(LabelX0, 6.0), TrackEditor->GetRowLabel(*Track).ToString(), GSTNTimelineColors::ReadableTextOn(Fill));
						OutDrawElements.PopClip();
					}
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
			// A track with several lanes (hitboxes) puts each lane's keys at their own height and draws them a little smaller
			const double LaneHalf = FGSTN_TimelineLayout::GetKeyHalfSize(*Data, TrackIndex);
			const double HalfSize = KeyFrame == Model->GetPlayhead() ? LaneHalf + 2.0 : LaneHalf;
			const FVector2D KeyCentre(KeyX, FGSTN_TimelineLayout::GetKeyY(*Data, TrackIndex, KeyIndex));
			if (Model->IsSelected(FGSTN_Selection::Key(TrackIndex, KeyIndex)))
			{
				DrawDiamond(OutDrawElements, LayerId + 9, AllottedGeometry, KeyCentre, HalfSize + 2.0, SelectionOutline());
			}
			DrawDiamond(OutDrawElements, LayerId + 10, AllottedGeometry, KeyCentre, HalfSize, FLinearColor(0.98f, 0.9f, 0.55f));
		}
	}

	// The frames the last validation flagged on this track: a thin line over the full height with a square on top, red for an error, yellow for a warning.
	// Errors are drawn after warnings so a frame with both shows red
	if (Findings.IsValid())
	{
		for (const EGSTN_Severity Severity : { EGSTN_Severity::Warning, EGSTN_Severity::Error })
		{
			for (const FGSTN_ValidationFinding& Finding : Findings->GetReport().Findings)
			{
				if (Finding.Severity != Severity || Finding.Where.TrackIndex != TrackIndex || Finding.Where.Frame == INDEX_NONE)
				{
					continue;
				}
				const double MarkerX = View.FrameToPixel(static_cast<double>(Finding.Where.Frame) + 0.5);
				if (MarkerX < -4.0 || MarkerX > Size.X + 4.0)
				{
					continue;
				}
				const FLinearColor MarkerColor = Severity == EGSTN_Severity::Error ? GSTNTimelineColors::FindingError() : GSTNTimelineColors::FindingWarning();
				DrawVerticalLine(OutDrawElements, LayerId + 10, AllottedGeometry, MarkerX, 0.0, Size.Y, MarkerColor, 2.0);
				DrawRect(OutDrawElements, LayerId + 10, AllottedGeometry, FVector2D(MarkerX - 3.0, 0.0), FVector2D(6.0, 6.0), MarkerColor);
			}
		}
	}

	// Playhead
	const double PlayX = View.FrameToPixel(Model->GetPlayhead());
	DrawRect(OutDrawElements, LayerId + 11, AllottedGeometry, FVector2D(PlayX, 0.0), FVector2D(FMath::Max(View.PixelsPerFrame, 2.0), Size.Y), PlayheadColor() * FLinearColor(1.f, 1.f, 1.f, 0.18f));
	DrawVerticalLine(OutDrawElements, LayerId + 11, AllottedGeometry, PlayX + 1.0, 0.0, Size.Y, PlayheadColor(), 2.0);

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
			DrawRect(OutDrawElements, LayerId + 12, AllottedGeometry, FVector2D(BoxMin.X, Top), FVector2D(BoxMax.X - BoxMin.X, Bottom - Top), BoxSelectFill());
			DrawVerticalLine(OutDrawElements, LayerId + 12, AllottedGeometry, BoxMin.X, Top, Bottom, BoxSelectLine());
			DrawVerticalLine(OutDrawElements, LayerId + 12, AllottedGeometry, BoxMax.X, Top, Bottom, BoxSelectLine());
			if (BoxMin.Y >= RowTop)
			{
				DrawRect(OutDrawElements, LayerId + 12, AllottedGeometry, FVector2D(BoxMin.X, Top), FVector2D(BoxMax.X - BoxMin.X, 1.0), BoxSelectLine());
			}
			if (BoxMax.Y <= RowTop + Size.Y)
			{
				DrawRect(OutDrawElements, LayerId + 12, AllottedGeometry, FVector2D(BoxMin.X, Bottom - 1.0), FVector2D(BoxMax.X - BoxMin.X, 1.0), BoxSelectLine());
			}
		}
	}

	if (PaintStats.IsValid())
	{
		PaintStats->AccumulatedSeconds += FPlatformTime::Seconds() - PaintStart;
	}
	return LayerId + 13;
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
	const FGSTN_LaneHit Hit = FGSTN_TimelineLayout::HitTestTrack(*Data, TrackIndex, Model->ViewTransform, Content.X, Content.Y - TrackIndex * FGSTN_TimelineLayout::RowHeight);
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
	const double LocalY = Content.Y - TrackIndex * FGSTN_TimelineLayout::RowHeight;
	if (Data != nullptr && !FGSTN_TimelineLayout::HitTestTrack(*Data, TrackIndex, Model->ViewTransform, Content.X, LocalY).IsHit())
	{
		// On a track with several lanes the double click adds the key to the lane under the cursor
		const int32 Lanes = FGSTN_TimelineLayout::GetLaneCount(*Data, TrackIndex);
		int32 Lane = INDEX_NONE;
		if (Lanes > 1)
		{
			double Nearest = TNumericLimits<double>::Max();
			for (int32 Candidate = 0; Candidate < Lanes; ++Candidate)
			{
				const double Y = FGSTN_TimelineLayout::GetLaneY(*Data, TrackIndex, Candidate);
				if (FMath::Abs(Y - LocalY) < Nearest)
				{
					Nearest = FMath::Abs(Y - LocalY);
					Lane = Candidate;
				}
			}
		}
		FGSTN_TimelineActions::AddAtFrame(*Model, TrackIndex, Model->ViewTransform.PixelToFrameIndex(Content.X), Lane);
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
	const EGSTN_LaneHitKind Kind = FGSTN_TimelineLayout::HitTestTrack(*Data, TrackIndex, Model->ViewTransform, Local.X, Local.Y).Kind;
	return (Kind == EGSTN_LaneHitKind::StartEdge || Kind == EGSTN_LaneHitKind::EndEdge) ? FCursorReply::Cursor(EMouseCursor::ResizeLeftRight) : FCursorReply::Unhandled();
}
