// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Validation/GSTN_ValidationReport.h"

/**
 * The findings of the last Validate of one timeline editor, kept so the Frame Data tab can list them: the same report that went to the
 * Message Log, not a second run. The toolkit sets it when Validate runs and marks it stale when the data changes afterwards (the list
 * says so instead of showing findings about a timeline that is no longer the one on screen). No widget in this header: a test reads it.
 */
class FGSTN_FindingsStore
{
public:
	/** Replaces the findings with the result of a Validate that just ran */
	void SetReport(const FGSTN_ValidationReport& InReport)
	{
		Report = InReport;
		bHasRun = true;
		bStale = false;
		OnChanged.Broadcast();
	}

	/** The data changed after the last Validate. Does nothing before the first one, or when it is already marked */
	void MarkStale()
	{
		if (bHasRun && !bStale)
		{
			bStale = true;
			OnChanged.Broadcast();
		}
	}

	const FGSTN_ValidationReport& GetReport() const { return Report; }
	bool HasRun() const { return bHasRun; }
	bool IsStale() const { return bStale; }

	/** One line above the list: what was found, or why there is nothing to show */
	FText GetHeadline() const;

	FSimpleMulticastDelegate OnChanged;

private:
	FGSTN_ValidationReport Report;
	bool bHasRun = false;
	bool bStale = false;
};
