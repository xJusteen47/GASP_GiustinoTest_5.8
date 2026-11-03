#include "GSTN_PartSetValidator.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsLimits.h"
#include "Data/GSTN_PartsValidation.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Misc/DataValidation.h"

bool UGSTN_PartSetValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && (InObject->IsA<UGSTN_PartSetDefinition>() || InObject->IsA<UGSTN_PenetrationTable>());
}

EDataValidationResult UGSTN_PartSetValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	return ValidateAsset(InAsset, Context);
}

EDataValidationResult UGSTN_PartSetValidator::ValidateAsset(const UObject* Asset, FDataValidationContext& Out, const TSet<FName>* KnownBones)
{
	if (!Asset)
	{
		return EDataValidationResult::NotValidated;
	}
	const UGSTN_PartSetDefinition* Set = Cast<UGSTN_PartSetDefinition>(Asset);
	if (!Set && !Asset->IsA<UGSTN_PenetrationTable>())
	{
		return EDataValidationResult::NotValidated;
	}

	// The asset's own pass keeps one source of truth for the rules; a caller's bone list needs the pure rules directly
	FDataValidationContext Local;
	if (Set && KnownBones)
	{
		FGSTN_PartsValidationReport Report;
		FGSTN_PartsValidation::ValidateParts(Set->Parts, Set->GrabGate, Set->BodyReaction, KnownBones, GSTN_PartsLimits::MaxParts, Report);
		for (const FGSTN_PartsIssue& Issue : Report.Issues)
		{
			if (Issue.bError)
			{
				Local.AddError(FText::FromString(Issue.Message));
			}
			else
			{
				Local.AddWarning(FText::FromString(Issue.Message));
			}
		}
	}
	else
	{
		Asset->IsDataValid(Local);
	}

	const FString Path = Asset->GetPathName();
	for (const FDataValidationContext::FIssue& Issue : Local.GetIssues())
	{
		const FText Message = FText::FromString(FString::Printf(TEXT("[%s] %s"), *Path, *Issue.Message.ToString()));
		if (Issue.Severity == EMessageSeverity::Error)
		{
			Out.AddError(Message);
		}
		else
		{
			Out.AddWarning(Message);
		}
	}
	return Local.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
