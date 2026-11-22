// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "Rules/GSTN_LinkRule_Drill.h"

#include "GSTN_SpinDrillTags.h"
#include "State/GSTN_LinkState.h"

FGSTN_LinkRuleResult UGSTN_LinkRule_Drill::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}
	if (bRefuseAllies && AreAllies(Context))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Team);
	}
	if (Context.Target.Tags.HasTag(GSTNSpinDrillTags::Immunity_Drill))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Requirements);
	}
	if (bRespectLatchImmunity && Context.Frame < Context.Target.LatchImmunityEndFrame)
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Immunity);
	}
	return FGSTN_LinkRuleResult::Accept();
}
