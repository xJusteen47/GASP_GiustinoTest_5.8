// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PenetrationTable.h"

#include "Data/GSTN_PartsLimits.h"
#include "Data/GSTN_PartsValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UGSTN_PenetrationTable::UGSTN_PenetrationTable()
{
	Bands = MakeDefaultBands();
}

TArray<FGSTN_PenetrationBand> UGSTN_PenetrationTable::MakeDefaultBands()
{
	// Hardness decides damage only (owner direction 2.h): above = full, equal = the Helldivers 65 %, below = ricochet. The
	// stagger and hitstop scales stay 1 in the damaging bands, so hardness never changes hitstun or hitstop
	FGSTN_PenetrationBand Above;
	Above.MinDelta = 1;

	FGSTN_PenetrationBand Equal;
	Equal.MinDelta = 0;
	Equal.PartScale = 0.65f;
	Equal.MainScale = 0.65f;

	FGSTN_PenetrationBand Below;
	Below.MinDelta = GSTN_PartsLimits::MinPossibleDelta;
	Below.PartScale = 0.f;
	Below.MainScale = 0.f;
	Below.StaggerScale = 0.f;
	Below.HitstopScale = 0.f;
	Below.bRicochet = true;

	return { Above, Equal, Below };
}

int32 UGSTN_PenetrationTable::LookupBandIndex(TConstArrayView<FGSTN_PenetrationBand> SortedBands, int32 Delta)
{
	for (int32 Index = 0; Index < SortedBands.Num(); ++Index)
	{
		if (Delta >= SortedBands[Index].MinDelta)
		{
			return Index;
		}
	}
	return SortedBands.IsEmpty() ? INDEX_NONE : SortedBands.Num() - 1;
}

int32 UGSTN_PenetrationTable::LookupBand(TConstArrayView<FGSTN_PenetrationBand> SortedBands, int32 Penetration, int32 Hardness, bool bIgnoreHardness)
{
	if (SortedBands.IsEmpty())
	{
		return INDEX_NONE;
	}
	return bIgnoreHardness ? 0 : LookupBandIndex(SortedBands, Penetration - Hardness);
}

void UGSTN_PenetrationTable::Normalize()
{
	Bands.StableSort([](const FGSTN_PenetrationBand& A, const FGSTN_PenetrationBand& B) { return A.MinDelta > B.MinDelta; });
}

void UGSTN_PenetrationTable::PostLoad()
{
	Super::PostLoad();
	Normalize();
}

#if WITH_EDITOR
void UGSTN_PenetrationTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Normalize();
}

EDataValidationResult UGSTN_PenetrationTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	FGSTN_PartsValidationReport Report;
	FGSTN_PartsValidation::ValidateBands(Bands, Report);
	for (const FGSTN_PartsIssue& Issue : Report.Issues)
	{
		if (Issue.bError)
		{
			Context.AddError(FText::FromString(Issue.Message));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Issue.Message));
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
