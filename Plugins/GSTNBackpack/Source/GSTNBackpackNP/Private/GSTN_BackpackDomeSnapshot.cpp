// Backpacks, Network Prediction half (M11 L7c: the dome blocks hits).

#include "GSTN_BackpackDomeSnapshot.h"

#include "GSTN_BackpackSim.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HitVictimView.h"
#include "Sync/GSTN_SyncFragment.h"

bool FGSTN_BackpackSnapshotExt::IsInside(int32 DomeOwnerId) const
{
	for (int32 Index = 0; Index < FMath::Min<int32>(NumInside, MaxInsideDomes); ++Index)
	{
		if (InsideDomes[Index] == DomeOwnerId)
		{
			return true;
		}
	}
	return false;
}

void FGSTN_BackpackSnapshotExt::AddInside(int32 DomeOwnerId)
{
	if (NumInside >= MaxInsideDomes || IsInside(DomeOwnerId))
	{
		return;
	}
	InsideDomes[NumInside++] = DomeOwnerId;
}

void FGSTN_BackpackSnapshotExt::ClearInside()
{
	NumInside = 0;
	for (int32& Id : InsideDomes)
	{
		Id = INDEX_NONE;
	}
}

const FGSTN_BackpackSnapshotExt* FGSTN_BackpackSnapshotExt::FromView(const FGSTN_HitVictimView& Victim)
{
	const FGSTN_BackpackSnapshotExt* Ext = static_cast<const FGSTN_BackpackSnapshotExt*>(Victim.FindSnapshotExtension(GSTNFeatureIds::Backpack));
	return Ext && Ext->Type == TypeWord ? Ext : nullptr;
}

const FGSTN_BackpackSnapshotExt* FGSTN_BackpackSnapshotExt::FromAttacker(const FGSTN_ResolvedHit& Incoming)
{
	const FGSTN_BackpackSnapshotExt* Ext = static_cast<const FGSTN_BackpackSnapshotExt*>(Incoming.FindAttackerExtension(GSTNFeatureIds::Backpack));
	return Ext && Ext->Type == TypeWord ? Ext : nullptr;
}

uint8 FGSTN_BackpackSnapshotContributor::GetTypeId() const
{
	return GSTNFeatureIds::Backpack;
}

bool FGSTN_BackpackSnapshotContributor::ReadDome(const FGSTN_SnapshotContributorContext& Context, FVector& OutCenter, double& OutRadius)
{
	const FGSTN_BackpackFragment* Fragment = Context.Fragments ? FGSTN_SyncFragmentList::Find<FGSTN_BackpackFragment>(*Context.Fragments) : nullptr;
	if (!Fragment || !Fragment->HasDome() || Fragment->DomeRadiusCm == 0 || Context.Frame < Fragment->DomeStartFrame)
	{
		return false;
	}
	// The fragment is the end of the previous frame: a dome whose frames end at this one is already down for it
	if (Fragment->DomeUntilFrame != INDEX_NONE && Context.Frame >= Fragment->DomeUntilFrame)
	{
		return false;
	}
	OutCenter = FVector(Fragment->DomeCenter);
	OutRadius = static_cast<double>(Fragment->DomeRadiusCm);
	return true;
}

FGSTN_ExtraHurtbox FGSTN_BackpackSnapshotContributor::MakeDomeHurtbox(const FVector& WorldCenter, double Radius, const FTransform& EntityTransform)
{
	FGSTN_ExtraHurtbox Extra;
	Extra.Shape.Type = EGSTN_ShapeType::Sphere;
	Extra.Shape.Extent = FVector(Radius);
	// World-fixed (the carrier at the raise frame) while extras are root-relative: placed back from this snapshot's transform, which the
	// history record keeps with the extra, so a rewind puts the shell where it was
	Extra.Shape.Local = FTransform(EntityTransform.InverseTransformPosition(WorldCenter));
	Extra.Priority = MAX_uint8;
	Extra.Source = GSTNFeatureIds::Backpack;
	Extra.bOutsideOnly = true;
	return Extra;
}

void FGSTN_BackpackSnapshotContributor::Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const
{
	FGSTN_BackpackSnapshotExt Ext;
	// A rebuild (victim drain) sees no other entry: the inside list of the build stands for the whole frame
	const FGSTN_BackpackSnapshotExt* Previous = Context.Previous ? Context.Previous->Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack) : nullptr;
	if (Previous && Previous->Type == FGSTN_BackpackSnapshotExt::TypeWord)
	{
		for (int32 Index = 0; Index < FMath::Min<int32>(Previous->NumInside, FGSTN_BackpackSnapshotExt::MaxInsideDomes); ++Index)
		{
			Ext.AddInside(Previous->InsideDomes[Index]);
		}
	}
	FVector Center;
	double Radius = 0.0;
	if (ReadDome(Context, Center, Radius))
	{
		Ext.bHasDome = 1;
		Ext.DomeCenter[0] = static_cast<float>(Center.X);
		Ext.DomeCenter[1] = static_cast<float>(Center.Y);
		Ext.DomeCenter[2] = static_cast<float>(Center.Z);
		Ext.DomeRadiusCm = static_cast<float>(Radius);
		// ReadDome found the fragment
		const FGSTN_BackpackFragment& Fragment = *FGSTN_SyncFragmentList::Find<FGSTN_BackpackFragment>(*Context.Fragments);
		Ext.PushSpeedQ = Fragment.DomePushSpeedQ;
		Ext.PushPacked = static_cast<uint8>((Fragment.DomePushFrames & 0x7F) | (Fragment.DomePushDirection == EGSTN_DomePushDirection::BackAlongAttack ? 0x80 : 0));
	}
	if (!Ext.HasDome() && Ext.NumInside == 0)
	{
		Blob.Reset();
		return;
	}
	Blob.Set(GSTNFeatureIds::Backpack, Ext);
}

void FGSTN_BackpackSnapshotContributor::AppendExtraHurtboxes(const FGSTN_SnapshotContributorContext& Context, FGSTN_ExtraHurtboxList& Out) const
{
	FVector Center;
	double Radius = 0.0;
	if (ReadDome(Context, Center, Radius))
	{
		Out.Add(MakeDomeHurtbox(Center, Radius, Context.Transform));
	}
}

void FGSTN_BackpackSnapshotContributor::FinalizeSnapshot(int32 Frame, TArrayView<FGSTN_SnapshotEntry> Entries) const
{
	struct FDome
	{
		int32 OwnerId = INDEX_NONE;
		FVector Center = FVector::ZeroVector;
		double RadiusSquared = 0.0;
	};
	// Entries are sorted by id: the domes come in ascending owner id, so every inside list is in the same order on every machine
	TArray<FDome, TInlineAllocator<4>> Domes;
	for (const FGSTN_SnapshotEntry& Entry : Entries)
	{
		const FGSTN_BackpackSnapshotExt* Ext = Entry.Extension.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
		if (Ext && Ext->Type == FGSTN_BackpackSnapshotExt::TypeWord && Ext->HasDome())
		{
			FDome& Dome = Domes.AddDefaulted_GetRef();
			Dome.OwnerId = Entry.EntityId;
			Dome.Center = Ext->GetDomeCenter();
			Dome.RadiusSquared = FMath::Square(static_cast<double>(Ext->DomeRadiusCm));
		}
	}
	// Almost every frame of a match stops here
	if (Domes.IsEmpty())
	{
		return;
	}
	for (FGSTN_SnapshotEntry& Entry : Entries)
	{
		// The side of an entity is its root, the same point the attacker tick tests the shell against
		const FVector Root = Entry.Transform.GetLocation();
		const FGSTN_BackpackSnapshotExt* Existing = Entry.Extension.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
		FGSTN_BackpackSnapshotExt Ext = Existing && Existing->Type == FGSTN_BackpackSnapshotExt::TypeWord ? *Existing : FGSTN_BackpackSnapshotExt();
		Ext.ClearInside();
		for (const FDome& Dome : Domes)
		{
			if (FVector::DistSquared(Root, Dome.Center) <= Dome.RadiusSquared)
			{
				Ext.AddInside(Dome.OwnerId);
			}
		}
		if (Ext.NumInside > 0 || (Existing && Existing->NumInside > 0))
		{
			Entry.Extension.Set(GSTNFeatureIds::Backpack, Ext);
		}
	}
}
