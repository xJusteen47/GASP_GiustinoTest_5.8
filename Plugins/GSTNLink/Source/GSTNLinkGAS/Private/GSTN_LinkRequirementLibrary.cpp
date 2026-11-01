// Links, ability-system half (no Network Prediction, no Mover).

#include "GSTN_LinkRequirementLibrary.h"

#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkTags.h"
#include "Input/GSTN_CommandBindings.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Rules/GSTN_LinkRule.h"

bool UGSTN_LinkRequirementLibrary::MeetsRequirements(const FGameplayTagContainer& Tags, const FGSTN_TagRequirements& Requirements)
{
	return Requirements.IsSatisfiedBy(Tags);
}

bool UGSTN_LinkRequirementLibrary::IsLinkLocked(const FGameplayTagContainer& Tags)
{
	return Tags.HasTag(GSTNLinkTags::Lock_Link);
}

FGSTN_LinkRuleResult UGSTN_LinkRequirementLibrary::EvaluateRequest(const FGSTN_LinkRequestContext& Context)
{
	const FGSTN_LinkRuleResult Common = UGSTN_LinkRule::CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}

	for (const TObjectPtr<UGSTN_LinkRule>& Rule : Context.Definition->Rules)
	{
		if (Rule == nullptr)
		{
			continue;
		}
		const FGSTN_LinkRuleResult Result = Rule->Evaluate(Context);
		if (!Result.bAccepted)
		{
			return Result;
		}
	}
	return FGSTN_LinkRuleResult::Accept();
}

void UGSTN_LinkRequirementLibrary::ResolveDefinitions(const UGSTN_LinkRegistry& Registry, FGSTN_LinkRequestContext& Context)
{
	Context.Definition = Registry.FindDefinition(Context.LinkType);
	if (Context.Definition == nullptr)
	{
		return;
	}

	const EGSTN_LinkChannel Other = Context.Definition->Channel == EGSTN_LinkChannel::Body ? EGSTN_LinkChannel::Hand : EGSTN_LinkChannel::Body;
	Context.Initiator.OtherChannelDefinition = Registry.GetDefinition(Context.Initiator.Links.Get(Other).LinkTypeIndex);
	Context.Target.OtherChannelDefinition = Registry.GetDefinition(Context.Target.Links.Get(Other).LinkTypeIndex);
}

void UGSTN_LinkRequirementLibrary::ApplyKitRules(const FGSTN_LinkKitExtension* InitiatorKit, FGSTN_LinkRequestContext& Context)
{
	Context.InitiatorGrabRules = InitiatorKit != nullptr ? &InitiatorKit->GrabRules : nullptr;
	Context.InitiatorLatchRules = InitiatorKit != nullptr ? &InitiatorKit->LatchRules : nullptr;
}

const FGSTN_LinkKitExtension* UGSTN_LinkRequirementLibrary::FindKitExtension(const UGSTN_PrimaryKitDefinition* Kit)
{
	return Kit != nullptr ? Kit->Extensions.Find<FGSTN_LinkKitExtension>() : nullptr;
}

void UGSTN_LinkRequirementLibrary::GetActionBlockingRoleTags(const UGSTN_LinkRegistry& Registry, FGameplayTagContainer& OutTags)
{
	for (const FGSTN_LinkTypeEntry& Entry : Registry.GetEntries())
	{
		if (Entry.Definition == nullptr)
		{
			continue;
		}
		for (int32 RoleIndex = 1; RoleIndex <= 2; ++RoleIndex)
		{
			const FGSTN_LinkRoleDef& Role = Entry.Definition->GetRoleDef(RoleIndex);
			if (Role.bParked && !Role.bActsWhileLinked && Role.RoleTag.IsValid())
			{
				OutTags.AddTag(Role.RoleTag);
			}
		}
	}
}
