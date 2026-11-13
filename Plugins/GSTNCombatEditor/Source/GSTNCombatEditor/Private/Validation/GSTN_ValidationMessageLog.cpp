// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_ValidationMessageLog.h"

#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Misc/MessageDialog.h"
#include "Misc/UObjectToken.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace GSTNValidationLog
{
	FName GetLogName()
	{
		return TEXT("GSTNCombat");
	}

	void RegisterListing()
	{
		FMessageLogModule& MessageLog = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		if (!MessageLog.IsRegisteredLogListing(GetLogName()))
		{
			MessageLog.RegisterLogListing(GetLogName(), LOCTEXT("MessageLogLabel", "GSTN Combat"));
		}
	}

	void UnregisterListing()
	{
		if (FMessageLogModule* MessageLog = FModuleManager::GetModulePtr<FMessageLogModule>("MessageLog"))
		{
			if (MessageLog->IsRegisteredLogListing(GetLogName()))
			{
				MessageLog->UnregisterLogListing(GetLogName());
			}
		}
	}

	EMessageSeverity::Type ToMessageSeverity(EGSTN_Severity Severity)
	{
		switch (Severity)
		{
		case EGSTN_Severity::Error: return EMessageSeverity::Error;
		case EGSTN_Severity::Warning: return EMessageSeverity::Warning;
		default: return EMessageSeverity::Info;
		}
	}

	FText FormatFinding(const FGSTN_ValidationFinding& Finding)
	{
		return FText::Format(LOCTEXT("FindingFormat", "[{0}] {1}"), FText::FromName(Finding.Code), Finding.Message);
	}

	TSharedRef<FActionToken> MakeShowAction(const FText& Detail)
	{
		return FActionToken::Create(LOCTEXT("ShowActionName", "Show"), LOCTEXT("ShowActionTip", "Shows the input patterns of the slots"),
			FOnActionTokenExecuted::CreateLambda([Detail]()
			{
				FMessageDialog::Open(EAppMsgType::Ok, Detail, LOCTEXT("ShowDialogTitle", "GSTN Combat validation"));
			}));
	}

	TSharedRef<FTokenizedMessage> MakeMessage(const FGSTN_ValidationFinding& Finding, bool bWithAssetLink)
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(ToMessageSeverity(Finding.Severity));
		if (bWithAssetLink)
		{
			if (UObject* Asset = Finding.Asset.Get())
			{
				Message->AddToken(FUObjectToken::Create(Asset));
			}
		}
		Message->AddToken(FTextToken::Create(FormatFinding(Finding)));
		if (!Finding.Detail.IsEmpty())
		{
			Message->AddToken(MakeShowAction(Finding.Detail));
		}
		return Message;
	}

	void WriteReport(const FGSTN_ValidationReport& Report, const FText& PageTitle, bool bOpenAlways, bool bOpenLog)
	{
		FMessageLog Log(GetLogName());
		Log.NewPage(PageTitle);
		if (Report.IsEmpty())
		{
			Log.Info(LOCTEXT("NothingFound", "No problems found."));
		}
		for (const FGSTN_ValidationFinding& Finding : Report.Findings)
		{
			Log.AddMessage(MakeMessage(Finding, true));
		}
		// Warnings and errors open the log; a clean run only when asked to, because the button should answer
		if (bOpenLog)
		{
			Log.Open(EMessageSeverity::Warning, bOpenAlways);
		}
	}
}

#undef LOCTEXT_NAMESPACE
