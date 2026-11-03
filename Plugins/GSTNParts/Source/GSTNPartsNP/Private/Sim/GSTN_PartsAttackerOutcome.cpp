// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsAttackerOutcome.h"

#include "Pipeline/GSTN_PartHitOutcome.h"

FName FGSTN_PartsAttackerOutcome::GetName() const
{
	return TEXT("GSTNPartsAttackerOutcome");
}

EGSTN_AttackerHitOutcome FGSTN_PartsAttackerOutcome::Evaluate(const FGSTN_HitContextView& View) const
{
	for (const FInstancedStruct& Entry : View.OutcomeExtensions)
	{
		if (const FGSTN_PartHitOutcome* Outcome = Entry.GetPtr<FGSTN_PartHitOutcome>())
		{
			return Outcome->HasFlag(GSTNPartOutcomeFlags::Ricochet) ? EGSTN_AttackerHitOutcome::Ricochet : EGSTN_AttackerHitOutcome::Normal;
		}
	}
	return EGSTN_AttackerHitOutcome::Normal;
}
