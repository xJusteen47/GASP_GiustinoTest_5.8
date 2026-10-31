// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Rules/GSTN_LinkRule.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkTags.h"

FGSTN_LinkRuleResult UGSTN_LinkRule::CheckCommon(const FGSTN_LinkRequestContext& Context)
{
	const UGSTN_LinkTypeDefinition* Definition = Context.Definition;
	if (Definition == nullptr)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::NoDefinition);
	}
	if (Context.Initiator.EntityId != INDEX_NONE && Context.Initiator.EntityId == Context.Target.EntityId)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::SelfLink);
	}

	// The initiator's own cooldown (after a throw): it cannot start another link before its end frame
	if (Context.Frame < Context.Initiator.CooldownEndFrame)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Cooldown);
	}

	// One slot per channel. A pending request also occupies the slot, so simultaneous requests on one entity cannot both win
	const FGSTN_LinkState& InitiatorSlot = Context.Initiator.Links.Get(Definition->Channel);
	if (InitiatorSlot.Phase != EGSTN_LinkPhase::None)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::InitiatorBusy);
	}
	const FGSTN_LinkState& TargetSlot = Context.Target.Links.Get(Definition->Channel);
	if (TargetSlot.Phase != EGSTN_LinkPhase::None)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::TargetBusy);
	}

	// A link on the other channel must coexist with this type (a Hand link and a grab do not, a Hand link and a mount do)
	if ((Context.Initiator.OtherChannelDefinition != nullptr && !Definition->CanCoexistWith(*Context.Initiator.OtherChannelDefinition))
		|| (Context.Target.OtherChannelDefinition != nullptr && !Definition->CanCoexistWith(*Context.Target.OtherChannelDefinition)))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::CoexistForbidden);
	}

	if (Context.Target.Tags.HasTagExact(GSTNLinkTags::Lock_Link))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::TargetLocked);
	}
	if (!Definition->RequestRequirements.IsSatisfiedBy(Context.Target.Tags))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::RequestRequirements);
	}
	return FGSTN_LinkRuleResult::Accept();
}

bool UGSTN_LinkRule::ContainsTagNamed(const FGameplayTagContainer& Tags, FName TagName)
{
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
	return Tag.IsValid() && Tags.HasTag(Tag);
}
