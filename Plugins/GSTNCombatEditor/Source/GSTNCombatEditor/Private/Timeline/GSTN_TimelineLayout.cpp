// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineLayout.h"

#include "Timeline/GSTN_KeyedTrackAccess.h"

FGSTN_Selection FGSTN_LaneHit::ToSelection() const
{
	switch (Kind)
	{
	case EGSTN_LaneHitKind::Body:
		return FGSTN_Selection::Window(TrackIndex, EGSTN_SelectionPart::Body);
	case EGSTN_LaneHitKind::StartEdge:
		return FGSTN_Selection::Window(TrackIndex, EGSTN_SelectionPart::Start);
	case EGSTN_LaneHitKind::EndEdge:
		return FGSTN_Selection::Window(TrackIndex, EGSTN_SelectionPart::End);
	case EGSTN_LaneHitKind::Key:
		return FGSTN_Selection::Key(TrackIndex, KeyIndex);
	default:
		return FGSTN_Selection();
	}
}

namespace
{
	const IGSTN_KeyedTrackAccess* FindLayoutKeyAccess(const FGSTN_CombatTimelineData& Data, int32 TrackIndex)
	{
		return Data.Tracks.IsValidIndex(TrackIndex) ? FGSTN_KeyedTrackAccessRegistry::Find(Data.Tracks[TrackIndex].GetScriptStruct()) : nullptr;
	}

	/** The x of the middle of a frame: where its key diamond sits */
	double LayoutFrameCentre(const FGSTN_TimelineViewTransform& View, int32 Frame)
	{
		return View.FrameToPixel(static_cast<double>(Frame) + 0.5);
	}
}

// Keys

int32 FGSTN_TimelineLayout::GetKeyCount(const FGSTN_CombatTimelineData& Data, int32 TrackIndex)
{
	const IGSTN_KeyedTrackAccess* Access = FindLayoutKeyAccess(Data, TrackIndex);
	const FGSTN_TimelineTrack* Track = Access ? Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>() : nullptr;
	return Track ? Access->GetKeyCount(*Track) : 0;
}

int32 FGSTN_TimelineLayout::GetKeyFrame(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, int32 KeyIndex)
{
	const IGSTN_KeyedTrackAccess* Access = FindLayoutKeyAccess(Data, TrackIndex);
	const FGSTN_TimelineTrack* Track = Access ? Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>() : nullptr;
	return (Track && KeyIndex >= 0 && KeyIndex < Access->GetKeyCount(*Track)) ? Access->GetKeyFrame(*Track, KeyIndex) : INDEX_NONE;
}

bool FGSTN_TimelineLayout::IsKeyedTrack(const FGSTN_CombatTimelineData& Data, int32 TrackIndex)
{
	return FindLayoutKeyAccess(Data, TrackIndex) != nullptr;
}

int32 FGSTN_TimelineLayout::GetEarliestSelectedKeyFrame(const FGSTN_CombatTimelineData& Data, TConstArrayView<FGSTN_Selection> Selection)
{
	int32 Earliest = INDEX_NONE;
	for (const FGSTN_Selection& Item : Selection)
	{
		if (!Item.IsKey())
		{
			continue;
		}
		const int32 Frame = GetKeyFrame(Data, Item.TrackIndex, Item.KeyIndex);
		if (Frame != INDEX_NONE && (Earliest == INDEX_NONE || Frame < Earliest))
		{
			Earliest = Frame;
		}
	}
	return Earliest;
}

// Hit testing

EGSTN_LaneHitKind FGSTN_TimelineLayout::HitTestWindow(int32 StartFrame, int32 EndFrame, bool bPoint, const FGSTN_TimelineViewTransform& View, double X)
{
	const double X0 = View.FrameToPixel(StartFrame);
	if (bPoint)
	{
		// A one-frame track is a diamond: all body, with a grab radius that does not vanish when zoomed out
		const double X1 = View.FrameToPixel(static_cast<double>(StartFrame) + 1.0);
		const double Half = FMath::Max(KeyHalfPixels, (X1 - X0) * 0.5);
		return FMath::Abs(X - (X0 + X1) * 0.5) <= Half ? EGSTN_LaneHitKind::Body : EGSTN_LaneHitKind::None;
	}

	// An empty or inverted window (the validator reports it) still gets a one-frame bar to grab
	const double X1 = FMath::Max(View.FrameToPixel(EndFrame), X0 + View.PixelsPerFrame);
	const double Tolerance = FMath::Min(EdgeGrabPixels, (X1 - X0) / 3.0);
	const bool bNearStart = X >= X0 - Tolerance && X <= X0 + Tolerance;
	const bool bNearEnd = X >= X1 - Tolerance && X <= X1 + Tolerance;
	if (bNearStart && bNearEnd)
	{
		return FMath::Abs(X - X0) <= FMath::Abs(X - X1) ? EGSTN_LaneHitKind::StartEdge : EGSTN_LaneHitKind::EndEdge;
	}
	if (bNearStart)
	{
		return EGSTN_LaneHitKind::StartEdge;
	}
	if (bNearEnd)
	{
		return EGSTN_LaneHitKind::EndEdge;
	}
	return (X > X0 && X < X1) ? EGSTN_LaneHitKind::Body : EGSTN_LaneHitKind::None;
}

FGSTN_LaneHit FGSTN_TimelineLayout::HitTestTrack(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, const FGSTN_TimelineViewTransform& View, double X)
{
	FGSTN_LaneHit Hit;
	const FGSTN_TimelineTrack* Track = Data.Tracks.IsValidIndex(TrackIndex) ? Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>() : nullptr;
	if (Track == nullptr)
	{
		return Hit;
	}
	Hit.TrackIndex = TrackIndex;

	// Keys sit on top of the bar: the nearest diamond within reach wins
	const int32 KeyCount = GetKeyCount(Data, TrackIndex);
	double BestDistance = KeyHalfPixels;
	for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		const double Distance = FMath::Abs(X - LayoutFrameCentre(View, GetKeyFrame(Data, TrackIndex, KeyIndex)));
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Hit.Kind = EGSTN_LaneHitKind::Key;
			Hit.KeyIndex = KeyIndex;
		}
	}
	if (Hit.Kind == EGSTN_LaneHitKind::Key)
	{
		return Hit;
	}

	Hit.Kind = HitTestWindow(Track->StartFrame, Track->EndFrame, FGSTN_TimelineEditorModel::IsPointTrack(*Track), View, X);
	if (Hit.Kind == EGSTN_LaneHitKind::None)
	{
		Hit.TrackIndex = INDEX_NONE;
	}
	return Hit;
}

TArray<FGSTN_Selection> FGSTN_TimelineLayout::BoxSelect(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineViewTransform& View, double X0, double Y0, double X1, double Y1)
{
	TArray<FGSTN_Selection> Result;
	const double MinX = FMath::Min(X0, X1);
	const double MaxX = FMath::Max(X0, X1);
	const int32 FirstRow = FMath::Max(FMath::FloorToInt32(FMath::Min(Y0, Y1) / RowHeight), 0);
	const int32 LastRow = FMath::Min(FMath::FloorToInt32(FMath::Max(Y0, Y1) / RowHeight), Data.Tracks.Num() - 1);

	for (int32 TrackIndex = FirstRow; TrackIndex <= LastRow; ++TrackIndex)
	{
		const FGSTN_TimelineTrack* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>();
		if (Track == nullptr)
		{
			continue;
		}

		bool bAnyKey = false;
		const int32 KeyCount = GetKeyCount(Data, TrackIndex);
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			const double Centre = LayoutFrameCentre(View, GetKeyFrame(Data, TrackIndex, KeyIndex));
			if (Centre >= MinX && Centre <= MaxX)
			{
				Result.Add(FGSTN_Selection::Key(TrackIndex, KeyIndex));
				bAnyKey = true;
			}
		}
		if (bAnyKey)
		{
			continue;
		}

		int32 Start = 0;
		int32 End = 0;
		FGSTN_TimelineEditorModel::GetTrackSpan(*Track, Start, End);
		const double WindowX0 = View.FrameToPixel(Start);
		const double WindowX1 = FMath::Max(View.FrameToPixel(End), WindowX0 + View.PixelsPerFrame);
		if (WindowX1 >= MinX && WindowX0 <= MaxX)
		{
			Result.Add(FGSTN_Selection::Window(TrackIndex));
		}
	}
	return Result;
}

// Ruler

FGSTN_RulerTicks FGSTN_TimelineLayout::ComputeRulerTicks(const FGSTN_TimelineViewTransform& View, double WidthPixels)
{
	static const int32 Steps[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000 };

	FGSTN_RulerTicks Ticks;
	const double PixelsPerFrame = FMath::Max(View.PixelsPerFrame, 0.001);
	Ticks.LabelStep = Steps[UE_ARRAY_COUNT(Steps) - 1];
	for (const int32 Step : Steps)
	{
		if (Step * PixelsPerFrame >= MinLabelSpacingPixels)
		{
			Ticks.LabelStep = Step;
			break;
		}
	}
	// A tenth of the label step from 10 up (10 -> 1, 20 -> 2, 50 -> 5): never closer than MinLabelSpacing / 10 pixels
	Ticks.MinorStep = Ticks.LabelStep >= 10 ? Ticks.LabelStep / 10 : 1;
	Ticks.FirstFrame = FMath::FloorToInt32(View.FirstVisibleFrame) - 1;
	Ticks.LastFrame = FMath::CeilToInt32(View.PixelToFrame(WidthPixels)) + 1;
	return Ticks;
}

FString FGSTN_TimelineLayout::FormatMilliseconds(int32 Frame, float TickRate)
{
	const double Milliseconds = TickRate > 0.f ? static_cast<double>(Frame) * 1000.0 / TickRate : 0.0;
	return FString::Printf(TEXT("%.1f"), Milliseconds);
}

// Navigation

int32 FGSTN_TimelineLayout::FindAdjacentEdge(const FGSTN_CombatTimelineData& Data, int32 Frame, int32 Direction)
{
	if (Direction == 0)
	{
		return INDEX_NONE;
	}

	int32 Best = INDEX_NONE;
	auto Consider = [&](int32 Candidate)
	{
		if (Direction > 0 ? Candidate > Frame : Candidate < Frame)
		{
			if (Best == INDEX_NONE || (Direction > 0 ? Candidate < Best : Candidate > Best))
			{
				Best = Candidate;
			}
		}
	};

	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack>();
		if (Track == nullptr)
		{
			continue;
		}
		int32 Start = 0;
		int32 End = 0;
		FGSTN_TimelineEditorModel::GetTrackSpan(*Track, Start, End);
		Consider(Start);
		Consider(FMath::Max(End - 1, Start));
		const int32 KeyCount = GetKeyCount(Data, TrackIndex);
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			Consider(GetKeyFrame(Data, TrackIndex, KeyIndex));
		}
	}
	return Best;
}

bool FGSTN_TimelineLayout::GetSelectionSpan(const FGSTN_CombatTimelineData& Data, TConstArrayView<FGSTN_Selection> Selection, int32& OutFirst, int32& OutEnd)
{
	bool bAny = false;
	auto Include = [&](int32 First, int32 End)
	{
		OutFirst = bAny ? FMath::Min(OutFirst, First) : First;
		OutEnd = bAny ? FMath::Max(OutEnd, End) : End;
		bAny = true;
	};

	for (const FGSTN_Selection& Item : Selection)
	{
		if (Item.IsKey())
		{
			const int32 Frame = GetKeyFrame(Data, Item.TrackIndex, Item.KeyIndex);
			if (Frame != INDEX_NONE)
			{
				Include(Frame, Frame + 1);
			}
		}
		else if (Data.Tracks.IsValidIndex(Item.TrackIndex))
		{
			if (const FGSTN_TimelineTrack* Track = Data.Tracks[Item.TrackIndex].GetPtr<FGSTN_TimelineTrack>())
			{
				int32 Start = 0;
				int32 End = 0;
				FGSTN_TimelineEditorModel::GetTrackSpan(*Track, Start, End);
				Include(Start, FMath::Max(End, Start + 1));
			}
		}
	}
	return bAny;
}

void FGSTN_TimelineLayout::ClampView(FGSTN_TimelineViewTransform& View, double WidthPixels, int32 TotalFrames)
{
	if (WidthPixels <= 0.0)
	{
		return;
	}
	const double Visible = View.GetVisibleFrameCount(WidthPixels);
	const double Lowest = -Visible * 0.25;
	const double Highest = FMath::Max(static_cast<double>(FMath::Max(TotalFrames, 1)) - Visible * 0.75, Lowest);
	View.FirstVisibleFrame = FMath::Clamp(View.FirstVisibleFrame, Lowest, Highest);
}

void FGSTN_TimelineLayout::ApplyWheel(FGSTN_TimelineViewTransform& View, double CursorX, double WheelDelta, bool bZoom, double WidthPixels, int32 TotalFrames)
{
	if (bZoom)
	{
		View.ZoomAround(CursorX, FMath::Pow(1.2, WheelDelta));
	}
	else
	{
		View.ScrollByPixels(-WheelDelta * 48.0);
	}
	ClampView(View, WidthPixels, TotalFrames);
}

void FGSTN_TimelineLayout::FrameAll(FGSTN_TimelineViewTransform& View, double WidthPixels, int32 TotalFrames)
{
	if (WidthPixels <= 0.0)
	{
		return;
	}
	// A timeline with no length yet shows a screenful of frames rather than a single enormous one
	const double Length = TotalFrames > 0 ? static_cast<double>(TotalFrames) : 30.0;
	View.FitRange(-0.5, Length + 0.5, WidthPixels);
}

void FGSTN_TimelineLayout::FrameRange(FGSTN_TimelineViewTransform& View, double WidthPixels, int32 First, int32 End)
{
	if (WidthPixels <= 0.0)
	{
		return;
	}
	// At least a few frames wide, with a frame of margin, so one key does not zoom in to the limit
	const double Span = FMath::Max(static_cast<double>(End - First), 6.0);
	const double Centre = (static_cast<double>(First) + static_cast<double>(End)) * 0.5;
	View.FitRange(Centre - Span * 0.5 - 1.0, Centre + Span * 0.5 + 1.0, WidthPixels);
}

// Playback

int32 FGSTN_PlaybackClock::Advance(double DeltaSeconds, float TickRate)
{
	if (TickRate <= 0.f || DeltaSeconds <= 0.0)
	{
		return 0;
	}
	Accumulator += FMath::Min(DeltaSeconds, MaxDeltaSeconds) * TickRate;
	const int32 Whole = FMath::FloorToInt32(Accumulator);
	Accumulator -= Whole;
	return Whole;
}

int32 FGSTN_PlaybackClock::StepLooped(int32 Frame, int32 Steps, int32 First, int32 End)
{
	const int32 Length = End - First;
	if (Length <= 0)
	{
		return First;
	}
	const int32 Offset = Frame - First + Steps;
	return First + ((Offset % Length) + Length) % Length;
}
