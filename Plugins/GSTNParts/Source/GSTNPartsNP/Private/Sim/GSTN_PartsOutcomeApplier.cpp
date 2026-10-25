// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsOutcomeApplier.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Effects/GSTN_PartBreakEffectApplier.h"
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

	// The one-shot Launch and Stagger of the parts this hit broke, asked of the victim step as a reaction of that strength (it merges them into the
	// hit's own, see FGSTN_ReactionRequest). The horizontal direction follows the hit (HitboxToVictim, the same on every machine) and the vertical one
	// is the victim's up, never world Z. Everything comes from the outcome entry and the static set, so a re-simulated hit asks for the same thing
	if (Outcome->NewlyBrokenMask != 0 && Ext && Ext->PartSet)
	{
		FGSTN_PartBreakRequests Requests;
		FGSTN_PartBreakEffectApplier::Collect(Ext->PartSet->GetParts(), Outcome->NewlyBrokenMask, View.Incoming.Up, View.Incoming.HitboxToVictim, 0, Requests);
		Context.Reaction.Merge(Requests.MinimumHitstunFrames, Requests.Launch.Velocity);
	}
}

void FGSTN_PartsOutcomeApplier::AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const
{
	if (const FGSTN_PartHitOutcome* Outcome = Extension.GetPtr<FGSTN_PartHitOutcome>())
	{
		FGSTN_PartEvents::Append(*Outcome, Envelope, Outbox);
	}
}
