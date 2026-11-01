// Links, Network Prediction half.

#include "Sim/GSTN_LinkSnapshotExt.h"

#include "Data/GSTN_GrabRules.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_HitVictimView.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Rules/GSTN_LinkRule_Latch.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sync/GSTN_SyncFragment.h"

namespace
{
	struct FRequestTagTable
	{
		TArray<FGameplayTag> Tags;
		uint32 Signature = 0;
		bool bBuilt = false;
	};

	FRequestTagTable& Table()
	{
		static FRequestTagTable Instance;
		return Instance;
	}

	void AddName(TArray<FGameplayTag>& Out, FName Name)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Name, false);
		if (Tag.IsValid())
		{
			Out.AddUnique(Tag);
		}
	}

	void AddContainer(TArray<FGameplayTag>& Out, const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			Out.AddUnique(Tag);
		}
	}

	void AddGrabRules(TArray<FGameplayTag>& Out, const FGSTN_GrabRules& Rules)
	{
		AddContainer(Out, Rules.Requirements.RequireTags);
		AddContainer(Out, Rules.Requirements.IgnoreTags);
		for (const FName& Name : Rules.IgnoreTagNames)
		{
			AddName(Out, Name);
		}
	}

	void Build(FRequestTagTable& Out)
	{
		TArray<FGameplayTag> Tags;
		const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
		AddGrabRules(Tags, FGSTN_GrabRules());
		AddContainer(Tags, FGSTN_LatchRules().Requirements.RequireTags);
		AddContainer(Tags, FGSTN_LatchRules().Requirements.IgnoreTags);
		if (Registry)
		{
			for (int32 Index = 1; Index <= Registry->Num(); ++Index)
			{
				const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(static_cast<uint8>(Index));
				if (!Definition)
				{
					continue;
				}
				AddContainer(Tags, Definition->RequestRequirements.RequireTags);
				AddContainer(Tags, Definition->RequestRequirements.IgnoreTags);
				for (const UGSTN_LinkRule* Rule : Definition->Rules)
				{
					if (const UGSTN_LinkRule_Grab* Grab = Cast<UGSTN_LinkRule_Grab>(Rule))
					{
						AddGrabRules(Tags, Grab->Rules);
					}
					else if (const UGSTN_LinkRule_Latch* Latch = Cast<UGSTN_LinkRule_Latch>(Rule))
					{
						AddContainer(Tags, Latch->Rules.Requirements.RequireTags);
						AddContainer(Tags, Latch->Rules.Requirements.IgnoreTags);
					}
				}
			}
		}
		// The generic ones first in importance, but the order is by name: the same table whatever the registration order
		Tags.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.GetTagName().LexicalLess(B.GetTagName()); });
		TArray<FGameplayTag> Generic;
		Generic.Add(GSTNLinkTags::Immunity_Grab);
		Generic.Add(GSTNLinkTags::Immunity_Latch);
		Generic.Add(GSTNLinkTags::Lock_Link);
		AddName(Generic, TEXT("Combat.Part.GrabLocked"));
		for (const FGameplayTag& Tag : Tags)
		{
			if (Generic.Num() >= FGSTN_LinkRequestTags::MaxTags)
			{
				break;
			}
			Generic.AddUnique(Tag);
		}
		Out.Tags = MoveTemp(Generic);
		Out.Signature = Registry ? Registry->GetSignature() : 0;
		Out.bBuilt = true;
	}
}

FGSTN_LinkStates FGSTN_LinkSnapshotExt::ToLinkStates() const
{
	FGSTN_LinkStates States;
	States.Body.LinkTypeIndex = BodyTypeIndex;
	States.Body.Role = BodyRole;
	States.Body.Phase = static_cast<EGSTN_LinkPhase>(BodyPhase);
	States.Body.Serial = BodySerial;
	States.Body.Flags = BodyFlags;
	States.Body.LinkFrame = LinkFrame;
	if (PartnerId != INDEX_NONE)
	{
		States.Body.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, static_cast<uint16>(PartnerId));
	}
	States.Hand.LinkTypeIndex = HandTypeIndex;
	States.Hand.Role = HandRole;
	States.Hand.Phase = HandRole != 0 ? EGSTN_LinkPhase::Active : EGSTN_LinkPhase::None;
	return States;
}

void FGSTN_LinkSnapshotExt::SetFromFragment(const FGSTN_LinkFragment& Fragment)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	BodyTypeIndex = Body.LinkTypeIndex;
	BodyRole = Body.Role;
	BodyPhase = static_cast<uint8>(Body.Phase);
	BodySerial = Body.Serial;
	BodyFlags = Body.Flags;
	LinkFrame = Body.LinkFrame;
	PartnerId = Body.PartnerId.IsNull() ? INDEX_NONE : static_cast<int32>(Body.PartnerId.Id);
	HandTypeIndex = Fragment.Links.Hand.LinkTypeIndex;
	HandRole = Fragment.Links.Hand.Role;
	GrabImmunityEndFrame = Fragment.GrabImmunityEndFrame;
	CooldownEndFrame = Fragment.Actions.CooldownEndFrame;
}

const FGSTN_LinkSnapshotExt* FGSTN_LinkSnapshotExt::FromVictim(const FGSTN_HitVictimView& Victim)
{
	const FGSTN_LinkSnapshotExt* Ext = static_cast<const FGSTN_LinkSnapshotExt*>(Victim.FindSnapshotExtension(GSTNFeatureIds::Link));
	return Ext && Ext->IsValid() ? Ext : nullptr;
}

const FGSTN_LinkSnapshotExt* FGSTN_LinkSnapshotExt::FromAttacker(const FGSTN_ResolvedHit& Incoming)
{
	const FGSTN_LinkSnapshotExt* Ext = static_cast<const FGSTN_LinkSnapshotExt*>(Incoming.FindAttackerExtension(GSTNFeatureIds::Link));
	return Ext && Ext->IsValid() ? Ext : nullptr;
}

const TArray<FGameplayTag>& FGSTN_LinkRequestTags::Get()
{
	FRequestTagTable& T = Table();
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!T.bBuilt || (Registry && Registry->GetSignature() != T.Signature))
	{
		Build(T);
	}
	return T.Tags;
}

uint32 FGSTN_LinkRequestTags::MakeMask(const FGameplayTagContainer& Tags)
{
	const TArray<FGameplayTag>& Known = Get();
	uint32 Mask = 0;
	for (int32 Index = 0; Index < Known.Num(); ++Index)
	{
		if (Tags.HasTagExact(Known[Index]))
		{
			Mask |= 1u << Index;
		}
	}
	return Mask;
}

void FGSTN_LinkRequestTags::AppendTags(uint32 Mask, FGameplayTagContainer& Out)
{
	const TArray<FGameplayTag>& Known = Get();
	for (int32 Index = 0; Index < Known.Num(); ++Index)
	{
		if (Mask & (1u << Index))
		{
			Out.AddTag(Known[Index]);
		}
	}
}

void FGSTN_LinkRequestTags::Invalidate()
{
	Table().bBuilt = false;
}
