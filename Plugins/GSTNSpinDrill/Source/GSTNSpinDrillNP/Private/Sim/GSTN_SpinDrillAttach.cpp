// Spin and drill, Network Prediction half.

#include "Sim/GSTN_SpinDrillAttach.h"

#include "GameFramework/Actor.h"
#include "UObject/ObjectKey.h"

namespace
{
	TMap<FObjectKey, FGSTN_DrillSimTuning>& Attached()
	{
		static TMap<FObjectKey, FGSTN_DrillSimTuning> Instance;
		return Instance;
	}

	/** Forgets the entries whose actor is gone, so a destroyed pawn does not keep an entry (only when something is attached: never in a tick) */
	void Prune()
	{
		for (auto It = Attached().CreateIterator(); It; ++It)
		{
			if (!It.Key().ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
	}
}

const FGSTN_DrillSimTuning* FGSTN_SpinDrillAttach::Find(const AActor* Owner)
{
	return Owner ? Attached().Find(FObjectKey(Owner)) : nullptr;
}

FGSTN_DrillSimTuning FGSTN_SpinDrillAttach::FindOrDefault(const AActor* Owner)
{
	const FGSTN_DrillSimTuning* Found = Find(Owner);
	return Found ? *Found : FGSTN_DrillSimTuning::MakeDefault();
}

void FGSTN_SpinDrillAttach::Apply(const AActor* Owner, const FGSTN_DrillSimTuning& Tuning)
{
	if (!Owner)
	{
		return;
	}
	Prune();
	Attached().Add(FObjectKey(Owner), Tuning);
}

void FGSTN_SpinDrillAttach::Clear(const AActor* Owner)
{
	if (Owner)
	{
		Attached().Remove(FObjectKey(Owner));
	}
}

int32 FGSTN_SpinDrillAttach::Num()
{
	return Attached().Num();
}

void FGSTN_SpinDrillAttach::Reset()
{
	Attached().Reset();
}
