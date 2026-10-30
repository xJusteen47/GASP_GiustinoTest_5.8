// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Rules/GSTN_LinkRule_Grab.h"

FGSTN_LinkRuleResult UGSTN_LinkRule_Grab::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}

	const FGSTN_GrabRules& Active = Context.InitiatorGrabRules != nullptr ? *Context.InitiatorGrabRules : Rules;
	const FGameplayTagContainer& TargetTags = Context.Target.Tags;

	if (!Active.Requirements.IsSatisfiedBy(TargetTags))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Requirements);
	}
	for (const FName& TagName : Active.IgnoreTagNames)
	{
		if (ContainsTagNamed(TargetTags, TagName))
		{
			return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Requirements);
		}
	}

	// The parts plugin projects this while its grab gate is unmet. Read by name: without that plugin the tag does not exist
	if (Active.bRespectGrabLocked && ContainsTagNamed(TargetTags, TEXT("Combat.Part.GrabLocked")))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::GrabLocked);
	}

	if (Context.Target.EffectiveWeight > Active.MaxGrabWeight)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::TooHeavy);
	}

	if (Context.Frame < Context.Target.GrabImmunityEndFrame)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Immunity);
	}
	return FGSTN_LinkRuleResult::Accept();
}
