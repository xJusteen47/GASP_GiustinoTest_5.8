// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Rules/GSTN_LinkRule_Latch.h"

FGSTN_LinkRuleResult UGSTN_LinkRule_Latch::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	// The shared checks refuse a target whose channel is busy, which is how one latcher per partner is enforced (A19)
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}

	const FGSTN_LatchRules& Active = Context.InitiatorLatchRules != nullptr ? *Context.InitiatorLatchRules : Rules;
	const FGameplayTagContainer& TargetTags = Context.Target.Tags;

	if (!Active.Requirements.IsSatisfiedBy(TargetTags))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Requirements);
	}
	if (Active.bRespectGrabLocked && ContainsTagNamed(TargetTags, TEXT("Combat.Part.GrabLocked")))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::GrabLocked);
	}

	// The mirror of the grab's limit: only a large enemy can be latched onto
	if (Context.Target.EffectiveWeight < Active.MinLatchWeight)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::TooLight);
	}

	if (Context.Frame < Context.Target.LatchImmunityEndFrame)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Immunity);
	}
	return FGSTN_LinkRuleResult::Accept();
}
