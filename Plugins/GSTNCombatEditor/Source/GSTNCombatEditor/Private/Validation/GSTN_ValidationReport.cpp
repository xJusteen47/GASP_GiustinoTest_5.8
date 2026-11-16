// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_ValidationReport.h"

FGSTN_ValidationFinding& FGSTN_ValidationReport::Add(EGSTN_Severity Severity, FName Code, FText Message, UObject* Asset, const FGSTN_FindingLocation& Where)
{
	FGSTN_ValidationFinding& Finding = Findings.AddDefaulted_GetRef();
	Finding.Severity = Severity;
	Finding.Code = Code;
	Finding.Message = MoveTemp(Message);
	Finding.Asset = Asset;
	Finding.Where = Where;
	return Finding;
}

void FGSTN_ValidationReport::Append(const FGSTN_ValidationReport& Other)
{
	Findings.Append(Other.Findings);
}

int32 FGSTN_ValidationReport::CountSeverity(EGSTN_Severity Severity) const
{
	int32 Count = 0;
	for (const FGSTN_ValidationFinding& Finding : Findings)
	{
		Count += Finding.Severity == Severity ? 1 : 0;
	}
	return Count;
}

int32 FGSTN_ValidationReport::CountCode(FName Code) const
{
	int32 Count = 0;
	for (const FGSTN_ValidationFinding& Finding : Findings)
	{
		Count += Finding.Code == Code ? 1 : 0;
	}
	return Count;
}

const FGSTN_ValidationFinding* FGSTN_ValidationReport::FindCode(FName Code) const
{
	return Findings.FindByPredicate([Code](const FGSTN_ValidationFinding& Finding) { return Finding.Code == Code; });
}

void FGSTN_ValidationReport::SetAssetWhereMissing(UObject* Asset)
{
	for (FGSTN_ValidationFinding& Finding : Findings)
	{
		if (!Finding.Asset.IsValid())
		{
			Finding.Asset = Asset;
		}
	}
}
