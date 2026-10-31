// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_FindingsStore.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FText FGSTN_FindingsStore::GetHeadline() const
{
	if (!bHasRun)
	{
		return LOCTEXT("FindingsNotRun", "Validate has not run yet: press Validate in the timeline toolbar (F7) to list the findings here.");
	}
	FText Counts = LOCTEXT("FindingsNone", "No problems found.");
	if (!Report.IsEmpty())
	{
		Counts = FText::Format(LOCTEXT("FindingsCounts", "{0} errors, {1} warnings, {2} notes."),
			FText::AsNumber(Report.CountSeverity(EGSTN_Severity::Error)), FText::AsNumber(Report.CountSeverity(EGSTN_Severity::Warning)), FText::AsNumber(Report.CountSeverity(EGSTN_Severity::Info)));
	}
	return bStale ? FText::Format(LOCTEXT("FindingsStale", "{0} The timeline changed since: press Validate again."), Counts) : Counts;
}

#undef LOCTEXT_NAMESPACE
