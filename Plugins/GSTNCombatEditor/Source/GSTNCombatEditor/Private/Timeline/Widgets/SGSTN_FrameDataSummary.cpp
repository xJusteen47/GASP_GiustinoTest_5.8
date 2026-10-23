// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_FrameDataSummary.h"

#include "Timeline/GSTN_PhaseBarData.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FText SGSTN_FrameDataSummary::MakeSummaryText(const FGSTN_TimelineEditorModel& Model)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (Data == nullptr)
	{
		return FText::GetEmpty();
	}
	const FGSTN_PhaseBarData Phases = FGSTN_PhaseBarData::Compute(*Data);
	const FText Summary = Phases.FormatSummary(Model.GetTickRate());
	if (Phases.TailStart == INDEX_NONE)
	{
		return Summary;
	}
	return FText::Format(LOCTEXT("SummaryWithTail", "{0} | Tail from {1} (blend out {2})"), Summary,
		FText::AsNumber(Phases.TailStart), FText::AsNumber(Phases.TailBlendOutFrames));
}

void SGSTN_FrameDataSummary::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	ChildSlot
	[
		SNew(STextBlock)
		.Text_Lambda([this]() { return MakeSummaryText(*Model); })
	];
}

#undef LOCTEXT_NAMESPACE
