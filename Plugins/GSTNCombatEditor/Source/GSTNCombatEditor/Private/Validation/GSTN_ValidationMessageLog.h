// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"
#include "Validation/GSTN_ValidationReport.h"

/**
 * Where a report goes to be read: the Message Log (category GSTNCombat) for the Validate button of the timeline editor, and tokenized
 * messages for Data Validation. The same finding text everywhere: "[Code] message", an asset link when the message stands alone, and a
 * Show action when the finding has a detail (the input patterns of a slot conflict).
 */
namespace GSTNValidationLog
{
	/** The Message Log category, as the spec names it */
	FName GetLogName();

	/** Registers the category with the Message Log so it has a tab. The module calls it at startup and UnregisterListing at shutdown */
	void RegisterListing();
	void UnregisterListing();

	EMessageSeverity::Type ToMessageSeverity(EGSTN_Severity Severity);

	/** "[Code] Message" */
	FText FormatFinding(const FGSTN_ValidationFinding& Finding);

	/** The Show action of a finding with a detail: a dialog with the detail text */
	TSharedRef<FActionToken> MakeShowAction(const FText& Detail);

	/**
	 * The finding as a message. bWithAssetLink puts a link to the asset first (the Message Log); Data Validation names the asset
	 * itself, so it passes false. A finding with a Detail gets a Show action that opens the detail in a dialog.
	 */
	TSharedRef<FTokenizedMessage> MakeMessage(const FGSTN_ValidationFinding& Finding, bool bWithAssetLink);

	/**
	 * Writes the report to the Message Log on a page of its own and opens the log when there is anything but infos to read (or always,
	 * with bOpenAlways). An empty report writes one info line saying nothing was found, so the button visibly did something.
	 */
	void WriteReport(const FGSTN_ValidationReport& Report, const FText& PageTitle, bool bOpenAlways);
}
