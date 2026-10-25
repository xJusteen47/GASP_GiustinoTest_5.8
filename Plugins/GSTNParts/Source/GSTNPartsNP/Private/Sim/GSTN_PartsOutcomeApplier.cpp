// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsOutcomeApplier.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Events/GSTN_PartEvents.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "State/GSTN_PartsFragment.h"
#include "Sync/GSTN_SyncFragment.h"

const UScriptStruct* FGSTN_PartsOutcomeApplier::GetExtensionStruct() const
{
	return FGSTN_PartHitOutcome::StaticStruct();
}

void FGSTN_PartsOutcomeApplier::Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const
{
	const FGSTN_PartHitOutcome* Outcome = Extension.GetPtr<FGSTN_PartHitOutcome>();
	if (!Outcome || !Context.VictimFragments)
	{
		return;
	}

	FGSTN_PartsFragment& Fragment = FGSTN_SyncFragmentList::FindOrAdd<FGSTN_PartsFragment>(*Context.VictimFragments);
	FGSTN_PartsState State = Outcome->GetResultState();
	// Monotonic: a break is only ever undone by an explicit restore, never by a hit built from an older view
	State.BrokenMask |= Fragment.BrokenMask;

	const FGSTN_PartsSnapshotExt* Ext = FGSTN_PartsSnapshotExt::FromView(View.Victim);
	const int32 PartCount = Ext && Ext->PartSet ? Ext->PartSet->GetParts().Num() : Fragment.PartCount;
	Fragment.SetState(State, PartCount);
}

void FGSTN_PartsOutcomeApplier::AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const
{
	if (const FGSTN_PartHitOutcome* Outcome = Extension.GetPtr<FGSTN_PartHitOutcome>())
	{
		FGSTN_PartEvents::Append(*Outcome, Envelope, Outbox);
	}
}
