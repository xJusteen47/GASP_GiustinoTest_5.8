// Links, Network Prediction half.

#include "Sim/GSTN_LinkSnapshotContributor.h"

#include "AbilitySystemComponent.h"
#include "Algo/BinarySearch.h"
#include "AbilitySystemGlobals.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "State/GSTN_LinkMath.h"

uint8 FGSTN_LinkSnapshotContributor::GetTypeId() const
{
	return GSTNFeatureIds::Link;
}

uint8 FGSTN_LinkSnapshotContributor::ComputeBits(const UGSTN_LinkTypeDefinition* Definition, uint8 Role, EGSTN_LinkPhase Phase)
{
	if (!Definition || Role == 0 || (Phase != EGSTN_LinkPhase::Active && Phase != EGSTN_LinkPhase::Exiting) || Definition->Channel != EGSTN_LinkChannel::Body)
	{
		return 0;
	}
	if (Definition->GetParkedRoleIndex() != Role)
	{
		return 0;
	}
	uint8 Bits = GSTNLinkSnapshotBits::Parked;
	if (!Definition->GetRoleDef(Role).bActsWhileLinked)
	{
		Bits |= GSTNLinkSnapshotBits::BlocksActions;
	}
	return Bits;
}

void FGSTN_LinkSnapshotContributor::Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const
{
	const FGSTN_LinkFragment* Fragment = Context.Fragments ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(*Context.Fragments) : nullptr;
	const FGSTN_LinkSnapshotExt* Previous = Context.Previous ? Context.Previous->Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link) : nullptr;
	const FGSTN_LinkBodyData* Body = FGSTN_LinkAttach::Find(Context.Owner);

	FGSTN_LinkSnapshotExt Ext;
	Ext.BodyIndex = Body ? Body->Index : (Previous ? Previous->BodyIndex : 0);
	if (Context.Owner)
	{
		// The tags of the end of the previous frame: the ability system ticked last in the frame the snapshot describes
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Context.Owner);
		Ext.RequestTagMask = AbilitySystem ? FGSTN_LinkRequestTags::MakeMask(AbilitySystem->GetOwnedGameplayTags()) : 0;
	}
	else if (Previous)
	{
		// A rebuild in the victim drain: the tags are those of the snapshot build
		Ext.RequestTagMask = Previous->RequestTagMask;
	}
	if (!Fragment && Ext.BodyIndex == 0 && Ext.RequestTagMask == 0)
	{
		return;
	}
	if (Fragment)
	{
		Ext.SetFromFragment(*Fragment);
		const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(Ext.BodyTypeIndex) : nullptr;
		Ext.Bits = ComputeBits(Definition, Ext.BodyRole, static_cast<EGSTN_LinkPhase>(Ext.BodyPhase));
	}
	Blob.Set(GSTNFeatureIds::Link, Ext);
}

bool FGSTN_LinkSnapshotContributor::ComputeParkedTransform(const FGSTN_LinkSnapshotExt& Ext, const FGSTN_SnapshotEntry& Partner, FTransform& OutTransform)
{
	if (!Ext.IsParked())
	{
		return false;
	}
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(Ext.BodyTypeIndex) : nullptr;
	if (!Definition)
	{
		return false;
	}
	// The partner's baked socket (its own body content), the root when it has none
	const FGSTN_LinkSnapshotExt* PartnerExt = Partner.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
	const FGSTN_LinkBodyData* PartnerBody = PartnerExt && PartnerExt->IsValid() ? FGSTN_LinkAttach::At(PartnerExt->BodyIndex) : nullptr;
	const FTransform Socket = PartnerBody ? PartnerBody->GetSocket(Definition->GetRoleDef(Ext.BodyRole).SocketOnPartner) : FTransform::Identity;
	FTransform PartnerTransform = Partner.Transform;
	PartnerTransform.SetScale3D(FVector::OneVector);
	return UGSTN_LinkMath::TryGetParkedTransform(*Definition, Ext.BodyRole, PartnerTransform, Socket, OutTransform);
}

void FGSTN_LinkSnapshotContributor::FinalizeSnapshot(int32 Frame, TArrayView<FGSTN_SnapshotEntry> Entries) const
{
	for (FGSTN_SnapshotEntry& Entry : Entries)
	{
		const FGSTN_LinkSnapshotExt* Ext = Entry.Extension.Get<FGSTN_LinkSnapshotExt>(GSTNFeatureIds::Link);
		if (!Ext || !Ext->IsValid() || !Ext->IsParked() || Ext->PartnerId == INDEX_NONE)
		{
			continue;
		}
		// Entries are sorted by id: the partner by binary search, the same on every machine
		const int32 PartnerIndex = Algo::BinarySearchBy(Entries, Ext->PartnerId, &FGSTN_SnapshotEntry::EntityId);
		if (PartnerIndex == INDEX_NONE)
		{
			// The partner is not hittable here (gone, or not registered yet): the entity keeps its own parked transform
			continue;
		}
		FTransform Parked;
		if (ComputeParkedTransform(*Ext, Entries[PartnerIndex], Parked))
		{
			Entry.Transform.SetLocation(Parked.GetLocation());
			Entry.Transform.SetRotation(Parked.GetRotation());
			Entry.Facing = Parked.GetRotation().GetForwardVector();
			Entry.Velocity = Entries[PartnerIndex].Velocity;
			// A parked role that acts (the latcher) attacks from where it rides, not from its capsule
			Entry.bPlacedByContributor = true;
		}
	}
}
