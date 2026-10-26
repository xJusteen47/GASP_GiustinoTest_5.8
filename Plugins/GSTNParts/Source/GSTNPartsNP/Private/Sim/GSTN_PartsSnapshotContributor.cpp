// Destructible parts, Network Prediction half.

#include "Sim/GSTN_PartsSnapshotContributor.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Math/GSTN_PartMath.h"
#include "Math/GSTN_PartsProjection.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Sim/GSTN_PartsAttach.h"
#include "State/GSTN_PartsFragment.h"

uint8 FGSTN_PartsSnapshotContributor::GetTypeId() const
{
	return GSTNFeatureIds::Parts;
}

void FGSTN_PartsSnapshotContributor::Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const
{
	// A rebuild inside the victim drain has no owner: the set it needs is the one the first build recorded
	const FGSTN_PartsSnapshotExt* Previous = Context.Previous ? Context.Previous->Get<FGSTN_PartsSnapshotExt>(GSTNFeatureIds::Parts) : nullptr;
	const UGSTN_PartSetDefinition* Set = Previous ? Previous->PartSet : FGSTN_PartsAttach::FindSet(Context.Owner);
	if (!Set || !Context.Fragments)
	{
		Blob.Reset();
		return;
	}

	FGSTN_PartsSnapshotExt Ext;
	Ext.PartSet = Set;
	if (const FGSTN_PartsFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(*Context.Fragments))
	{
		Ext.SetState(Fragment->ToState());
	}
	if (UGSTN_PartMath::IsGrabLocked(Set->GrabGate, Ext.BrokenMask))
	{
		Ext.Flags |= FGSTN_PartsSnapshotExt::FlagGrabLocked;
	}
	Blob.Set(GSTNFeatureIds::Parts, Ext);
}

void FGSTN_PartsSnapshotContributor::ModifyEntry(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotEntry& Entry) const
{
	const FGSTN_PartsSnapshotExt* Ext = Entry.Extension.Get<FGSTN_PartsSnapshotExt>(GSTNFeatureIds::Parts);
	if (!Ext || !Ext->PartSet || Ext->BrokenMask == 0 || !Entry.Hurtboxes || Entry.HurtStates.IsEmpty())
	{
		return;
	}

	FGSTN_PartsProjection Projection;
	UGSTN_PartMath::ProjectDerived(Ext->PartSet->GetParts(), Ext->ToState(), Ext->PartSet->GrabGate, Projection);
	if (Projection.DisabledHurtboxes.IsEmpty())
	{
		return;
	}
	const TArray<FGSTN_Hurtbox>& Boxes = Entry.Hurtboxes->Hurtboxes;
	for (int32 Index = 0; Index < Boxes.Num() && Index < Entry.HurtStates.Num(); ++Index)
	{
		if (Projection.DisabledHurtboxes.Contains(Boxes[Index].Name))
		{
			Entry.HurtStates[Index].State = EGSTN_HurtState::Intangible;
		}
	}
}
