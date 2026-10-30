// Defence, Network Prediction half.

#include "Sim/GSTN_DefenceSnapshotContributor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "Data/GSTN_DefenceRules.h"
#include "GameFramework/Actor.h"
#include "GSTN_DefenceLibrary.h"
#include "GSTN_DefenceTags.h"
#include "Math/GSTN_DefenceMath.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"

const FGSTN_DefenceSnapshotExt* FGSTN_DefenceSnapshotExt::FromView(const FGSTN_HitVictimView& Victim)
{
	const FGSTN_DefenceSnapshotExt* Ext = static_cast<const FGSTN_DefenceSnapshotExt*>(Victim.FindSnapshotExtension(GSTNFeatureIds::Defence));
	return Ext && Ext->Type == TypeWord ? Ext : nullptr;
}

uint8 FGSTN_DefenceSnapshotContributor::GetTypeId() const
{
	return GSTNFeatureIds::Defence;
}

bool FGSTN_DefenceSnapshotContributor::BubbleExistsAt(const FGSTN_DefenceFragment& Fragment, int32 Frame, int32 ReleaseFrames)
{
	if (Fragment.IsBreaking())
	{
		return false;
	}
	if (Fragment.IsShieldStunned(Frame))
	{
		// The bubble stays up through the stun: further hits still land on it
		return true;
	}
	if (Fragment.ShieldPressFrame == INDEX_NONE || Frame < Fragment.ShieldPressFrame)
	{
		return false;
	}
	if (Fragment.ShieldReleaseFrame == INDEX_NONE)
	{
		return Fragment.IsRaised();
	}
	return Frame < Fragment.ShieldReleaseFrame + FMath::Max(ReleaseFrames, 0);
}

bool FGSTN_DefenceSnapshotContributor::BuildExtension(const FGSTN_SnapshotContributorContext& Context, FGSTN_DefenceSnapshotExt& OutExt, const FGSTN_DefenceEntityData*& OutData)
{
	OutExt = FGSTN_DefenceSnapshotExt();
	OutData = FGSTN_DefenceAttach::Find(Context.Owner);
	const FGSTN_DefenceSnapshotExt* Previous = Context.Previous ? Context.Previous->Get<FGSTN_DefenceSnapshotExt>(GSTNFeatureIds::Defence) : nullptr;
	const UGSTN_DefenceRules* Rules = OutData ? OutData->Rules : (Previous ? FGSTN_DefenceAttach::RulesAt(Previous->RulesIndex) : nullptr);
	if (!Rules)
	{
		return false;
	}
	const int32 Frame = Context.Frame;
	const FGSTN_DefenceFragment* Fragment = Context.Fragments ? FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(*Context.Fragments) : nullptr;
	const FGSTN_DefenceFragment Defaults;
	const FGSTN_DefenceFragment& Frag = Fragment ? *Fragment : Defaults;

	// The content and the entity's tags: from the owner at a build, from the earlier blob at a rebuild (which has no owner)
	uint8 Flags = 0;
	uint8 MultiplierQ = 16;
	FGSTN_ShieldRates Rates = FGSTN_ShieldRates::FromRules(Rules->Shield);
	if (OutData)
	{
		Flags |= Rules->bShieldEnabled ? GSTNDefenceSnapshotFlags::ShieldEnabled : 0;
		Flags |= OutData->bKitHasShield ? GSTNDefenceSnapshotFlags::KitHasShield : 0;
		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Context.Owner))
		{
			Flags |= ASC->HasMatchingGameplayTag(GSTNDefenceTags::Immunity_Parry) ? GSTNDefenceSnapshotFlags::ImmuneParry : 0;
			Flags |= ASC->HasMatchingGameplayTag(GSTNDefenceTags::Immunity_Shield) ? GSTNDefenceSnapshotFlags::ImmuneShield : 0;
			if (const UGSTN_DefenceAttributeSet* Set = UGSTN_DefenceLibrary::FindDefenceAttributeSet(ASC))
			{
				Rates = Set->ReadShieldRates(Rules->Shield.RegenDelayFrames);
				MultiplierQ = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Set->GetIncomingShieldDamageMultiplier() * 16.f), 0, 255));
			}
		}
		OutExt.RulesIndex = OutData->RulesIndex;
	}
	else if (Previous)
	{
		Flags |= Previous->Flags & (GSTNDefenceSnapshotFlags::ShieldEnabled | GSTNDefenceSnapshotFlags::KitHasShield | GSTNDefenceSnapshotFlags::ImmuneParry | GSTNDefenceSnapshotFlags::ImmuneShield);
		MultiplierQ = Previous->ShieldDamageMultiplierQ;
		OutExt.RulesIndex = Previous->RulesIndex;
	}
	const bool bAvailable = (Flags & GSTNDefenceSnapshotFlags::ShieldEnabled) && (Flags & GSTNDefenceSnapshotFlags::KitHasShield);

	// The shield at the frame
	Flags |= Frag.IsRaised() ? GSTNDefenceSnapshotFlags::Raised : 0;
	Flags |= Frag.IsBreaking() ? GSTNDefenceSnapshotFlags::Breaking : 0;
	const bool bPastStartup = Frag.ShieldPressFrame != INDEX_NONE && Frame >= Frag.ShieldPressFrame + FMath::Max(Rules->Shield.StartupFrames, 0);
	Flags |= bAvailable && Frag.IsRaised() && !Frag.IsBreaking() && bPastStartup ? GSTNDefenceSnapshotFlags::Blocks : 0;
	const bool bInReaction = Context.ActionState == EGSTN_ActionState::Hitstun || Context.ActionState == EGSTN_ActionState::Tumble
		|| Context.ActionState == EGSTN_ActionState::Splat || Context.ActionState == EGSTN_ActionState::KO;
	Flags |= bAvailable && !bInReaction && BubbleExistsAt(Frag, Frame, Rules->Shield.ReleaseFrames) ? GSTNDefenceSnapshotFlags::Bubble : 0;
	OutExt.Flags = Flags;
	OutExt.ShieldDamageMultiplierQ = MultiplierQ;
	OutExt.ShieldPressFrame = Frag.ShieldPressFrame;
	OutExt.ShieldReleaseFrame = Frag.ShieldReleaseFrame;
	// At a build the value is the analytic one at the frame; at a rebuild the stamp the applier just wrote is the frame's value
	OutExt.ShieldValueQ = OutData ? FGSTN_DefenceFragment::QuantizeValue(UGSTN_DefenceMath::ShieldValueAt(Frag.MakeShieldView(Rates), Frame, Rates))
		: (Frag.ShieldStampFrame == INDEX_NONE ? FGSTN_DefenceFragment::QuantizeValue(Rates.Max) : Frag.ShieldValueQ);

	// The move's parry window, whatever the mode
	OutExt.MoveParryStartFrame = Frag.MoveParryStartFrame;
	OutExt.MoveParryEndFrame = Frag.MoveParryEndFrame;
	OutExt.MoveParryProfile = Frag.MoveParryProfile;
	OutExt.MoveParryHurtboxMask = Frag.MoveParryHurtboxMask;
	return true;
}

void FGSTN_DefenceSnapshotContributor::Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const
{
	FGSTN_DefenceSnapshotExt Ext;
	const FGSTN_DefenceEntityData* Data = nullptr;
	if (BuildExtension(Context, Ext, Data))
	{
		Blob.Set(GSTNFeatureIds::Defence, Ext);
	}
	else
	{
		Blob.Reset();
	}
}

void FGSTN_DefenceSnapshotContributor::AppendExtraHurtboxes(const FGSTN_SnapshotContributorContext& Context, FGSTN_ExtraHurtboxList& Out) const
{
	FGSTN_DefenceSnapshotExt Ext;
	const FGSTN_DefenceEntityData* Data = nullptr;
	if (!BuildExtension(Context, Ext, Data) || !Data || !Data->Rules || !Ext.Has(GSTNDefenceSnapshotFlags::Bubble))
	{
		return;
	}
	Out.Add(UGSTN_DefenceMath::MakeShieldBubbleHurtbox(Data->Rules->Shield, Data->BubbleIgnoreHitTypes));
}
