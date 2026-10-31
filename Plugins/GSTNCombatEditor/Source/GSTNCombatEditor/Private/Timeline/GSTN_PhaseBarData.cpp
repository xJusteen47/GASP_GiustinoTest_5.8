// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_PhaseBarData.h"

#include "Timeline/GSTN_TimelinePhases.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FGSTN_PhaseBarData FGSTN_PhaseBarData::Compute(const FGSTN_CombatTimelineData& Data)
{
	// The numbers are Core's (FGSTN_TimelinePhases): this only lays them out as bar segments
	const FGSTN_TimelinePhases Phases = FGSTN_TimelinePhases::Compute(Data);

	FGSTN_PhaseBarData Result;
	Result.TotalFrames = Phases.Total;
	Result.FirstActionableFrame = Phases.FirstActionableFrame;
	Result.ActiveRanges = Phases.ActiveRanges;
	Result.bHasActive = Phases.bHasActive;
	Result.StartupFrames = Phases.GetStartupFrames();
	Result.ActiveFrames = Phases.GetActiveFrames();
	Result.RecoveryFrames = Phases.GetRecoveryFrames();
	Result.TailStart = Phases.TailStart;
	Result.TailBlendOutFrames = Phases.TailBlendOutFrames;

	const int32 Faf = FMath::Max(Phases.FirstActionableFrame, 0);
	const int32 End = FMath::Max(Phases.Total, Faf);
	auto AddSegment = [&Result](int32 Start, int32 SegmentEnd, EGSTN_PhaseKind Kind)
	{
		if (SegmentEnd > Start)
		{
			Result.Segments.Add({ Start, SegmentEnd, Kind });
		}
	};

	if (Phases.bHasActive)
	{
		AddSegment(0, Phases.ActiveStart, EGSTN_PhaseKind::Startup);
		int32 Cursor = Phases.ActiveStart;
		for (const FIntPoint& Range : Phases.ActiveRanges)
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
	return Result;
}

FText FGSTN_PhaseBarData::FormatSummary(float TickRate) const
{
	// Same line as FGSTN_TimelinePhases::FormatSummary (Core), rebuilt from the copied numbers so the bar needs no second Compute
	FGSTN_TimelinePhases Phases;
	Phases.Total = TotalFrames;
	Phases.FirstActionableFrame = FirstActionableFrame;
	Phases.RecoveryEnd = FirstActionableFrame;
	Phases.bHasActive = bHasActive;
	if (bHasActive)
	{
		Phases.ActiveStart = StartupFrames;
		Phases.StartupEnd = StartupFrames;
		Phases.ActiveEnd = StartupFrames + ActiveFrames;
	}
	return Phases.FormatSummary(TickRate);
}

#undef LOCTEXT_NAMESPACE
