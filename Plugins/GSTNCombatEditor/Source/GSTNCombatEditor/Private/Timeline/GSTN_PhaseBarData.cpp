// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_PhaseBarData.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FGSTN_PhaseBarData FGSTN_PhaseBarData::Compute(const FGSTN_CombatTimelineData& Data)
{
	FGSTN_PhaseBarData Result;
	Result.TotalFrames = Data.TotalFrames;
	Result.FirstActionableFrame = Data.FirstActionableFrame;

	// Merge the hitbox windows: sorted by start, overlapping or touching ranges become one
	TArray<FIntPoint> Windows;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>();
		if (Track != nullptr && Track->GetKind() == EGSTN_TimelineTrackKind::Hitbox && Track->EndFrame > Track->StartFrame)
		{
			Windows.Add(FIntPoint(FMath::Max(Track->StartFrame, 0), Track->EndFrame));
		}
	}
	Windows.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X < B.X; });
	for (const FIntPoint& Window : Windows)
	{
		if (!Result.ActiveRanges.IsEmpty() && Window.X <= Result.ActiveRanges.Last().Y)
		{
			Result.ActiveRanges.Last().Y = FMath::Max(Result.ActiveRanges.Last().Y, Window.Y);
		}
		else
		{
			Result.ActiveRanges.Add(Window);
		}
	}
	Result.bHasActive = !Result.ActiveRanges.IsEmpty();

	const int32 Faf = FMath::Max(Data.FirstActionableFrame, 0);
	const int32 End = FMath::Max(Data.TotalFrames, Faf);
	auto AddSegment = [&Result](int32 Start, int32 SegmentEnd, EGSTN_PhaseKind Kind)
	{
		if (SegmentEnd > Start)
		{
			Result.Segments.Add({ Start, SegmentEnd, Kind });
		}
	};

	if (Result.bHasActive)
	{
		const int32 ActiveStart = Result.ActiveRanges[0].X;
		const int32 ActiveEnd = Result.ActiveRanges.Last().Y;
		Result.StartupFrames = ActiveStart;
		Result.ActiveFrames = ActiveEnd - ActiveStart;
		Result.RecoveryFrames = FMath::Max(Faf - ActiveEnd, 0);

		AddSegment(0, ActiveStart, EGSTN_PhaseKind::Startup);
		int32 Cursor = ActiveStart;
		for (const FIntPoint& Range : Result.ActiveRanges)
		{
			AddSegment(Cursor, Range.X, EGSTN_PhaseKind::Recovery);
			AddSegment(Range.X, Range.Y, EGSTN_PhaseKind::Active);
			Cursor = Range.Y;
		}
		AddSegment(Cursor, Faf, EGSTN_PhaseKind::Recovery);
	}
	else
	{
		AddSegment(0, Faf, EGSTN_PhaseKind::Busy);
	}

	// Drawn left to right, so an active range that runs past the actionable frame is painted over by it
	AddSegment(Faf, End, EGSTN_PhaseKind::Actionable);

	Result.TailStart = Data.GetTailStartFrame();
	Result.TailBlendOutFrames = Data.Tail.ResolveBlendOutFrames(Data.TotalFrames, Data.FirstActionableFrame);
	return Result;
}

FText FGSTN_PhaseBarData::FormatSummary(float TickRate) const
{
	const int32 Milliseconds = TickRate > 0.f ? FMath::RoundToInt(static_cast<double>(TotalFrames) * 1000.0 / TickRate) : 0;
	if (!bHasActive)
	{
		return FText::Format(LOCTEXT("SummaryNoHitbox", "No hitbox | FAF {0} | Total {1} ({2} ms)"),
			FText::AsNumber(FirstActionableFrame), FText::AsNumber(TotalFrames), FText::AsNumber(Milliseconds));
	}
	return FText::Format(LOCTEXT("Summary", "Startup {0} | Active {1} | Recovery {2} | FAF {3} | Total {4} ({5} ms)"),
		FText::AsNumber(StartupFrames), FText::AsNumber(ActiveFrames), FText::AsNumber(RecoveryFrames),
		FText::AsNumber(FirstActionableFrame), FText::AsNumber(TotalFrames), FText::AsNumber(Milliseconds));
}

#undef LOCTEXT_NAMESPACE
