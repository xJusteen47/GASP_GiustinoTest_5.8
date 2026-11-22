// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Rules/GSTN_LinkRule_Team.h"

FGSTN_LinkRuleResult UGSTN_LinkRule_Team::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}
	const bool bAllies = AreAllies(Context);
	if ((Requirement == EGSTN_LinkTeamRequirement::AlliesOnly && !bAllies) || (Requirement == EGSTN_LinkTeamRequirement::EnemiesOnly && bAllies))
	{
		return FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Team);
	}
	return FGSTN_LinkRuleResult::Accept();
}
