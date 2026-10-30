// Combat simulation (GAS-NPP fork): the snapshot extension seam (spec seam D9).

#include "World/GSTN_StepSnapshot.h"

FGSTN_SnapshotContributorRegistry& FGSTN_SnapshotContributorRegistry::Get()
{
	static FGSTN_SnapshotContributorRegistry Registry;
	return Registry;
}

bool FGSTN_SnapshotContributorRegistry::Register(IGSTN_SnapshotContributor* Contributor)
{
	if (!Contributor || Contributor->GetTypeId() == 0 || Contributors.Contains(Contributor))
	{
		return false;
	}
	for (const IGSTN_SnapshotContributor* Other : Contributors)
	{
		if (Other->GetTypeId() == Contributor->GetTypeId())
		{
			return false;
		}
	}
	// The entry and the victim view hold MaxContributors blobs: one more would be silently invisible to the steps, so refuse it loudly
	if (!ensureMsgf(Contributors.Num() < MaxContributors, TEXT("Only %d snapshot extension type(s) are supported at once"), MaxContributors))
	{
		return false;
	}
	// Ascending type id: the order every machine fills the blob in, whatever the load order of the plugins
	int32 At = 0;
	while (At < Contributors.Num() && Contributors[At]->GetTypeId() < Contributor->GetTypeId())
	{
		++At;
	}
	Contributors.Insert(Contributor, At);
	return true;
}

void FGSTN_SnapshotContributorRegistry::Unregister(IGSTN_SnapshotContributor* Contributor)
{
	Contributors.Remove(Contributor);
}

void FGSTN_SnapshotContributorRegistry::Fill(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotEntry& Entry) const
{
	Entry.Extension.Reset();
	Entry.ExtraHurtboxes.Reset();
	for (const IGSTN_SnapshotContributor* Contributor : Contributors)
	{
		// Each contributor writes its own blob into the slot of its id; what it reads as Previous is its own earlier blob, never another's
		const uint8 TypeId = Contributor->GetTypeId();
		FGSTN_SnapshotExtensionBlob Blob;
		Contributor->Contribute(Context, Blob);
		Entry.Extension.Assign(TypeId, Blob);
		if (Blob.IsSet())
		{
			Contributor->ModifyEntry(Context, Entry);
		}
		Contributor->AppendExtraHurtboxes(Context, Entry.ExtraHurtboxes);
	}
	// The index of a hit holds two extras: the ones past that would be silently unhittable, and the same cut on every machine keeps them equal
	if (Entry.ExtraHurtboxes.Num() > GSTNHurtboxIndex::MaxExtraHurtboxes)
	{
		Entry.ExtraHurtboxes.SetNum(GSTNHurtboxIndex::MaxExtraHurtboxes, EAllowShrinking::No);
	}
}

double GSTNExtraHurtboxes::ComputeBoundRadius(TConstArrayView<FGSTN_ExtraHurtbox> Extras)
{
	double Radius = 0.0;
	for (const FGSTN_ExtraHurtbox& Extra : Extras)
	{
		const FTransform& Local = Extra.Shape.Local;
		Radius = FMath::Max(Radius, Local.GetLocation().Size() + Extra.Shape.Extent.GetMax() * Local.GetScale3D().GetAbsMax());
	}
	return Radius;
}

void FGSTN_SnapshotContributorRegistry::Rebuild(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionSet& Set) const
{
	for (const IGSTN_SnapshotContributor* Contributor : Contributors)
	{
		const uint8 TypeId = Contributor->GetTypeId();
		FGSTN_SnapshotContributorContext Own = Context;
		Own.Previous = Set.Find(TypeId);
		FGSTN_SnapshotExtensionBlob Blob;
		Contributor->Contribute(Own, Blob);
		Set.Assign(TypeId, Blob);
	}
}

void FGSTN_SnapshotContributorRegistry::Finalize(int32 Frame, TArrayView<FGSTN_SnapshotEntry> Entries) const
{
	for (const IGSTN_SnapshotContributor* Contributor : Contributors)
	{
		Contributor->FinalizeSnapshot(Frame, Entries);
	}
}
