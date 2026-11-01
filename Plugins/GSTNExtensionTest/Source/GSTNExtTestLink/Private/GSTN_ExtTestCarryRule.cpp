#include "GSTN_ExtTestCarryRule.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNExtTestLink)

namespace GSTNExtTestLinkTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Type_Carry, "Combat.Link.Type.GSTNExtTestCarry", "Test only: a carry link type added by the extension test plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Carrier, "Combat.Link.Role.GSTNExtTestCarrier", "Test only: the carry's initiator, keeps acting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Carried, "Combat.Link.Role.GSTNExtTestCarried", "Test only: the carry's target, parked on the carrier");
}

FGSTN_LinkRuleResult UGSTN_ExtTestCarryRule::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}
	return Context.Target.EffectiveWeight <= MaxCarryWeight ? FGSTN_LinkRuleResult::Accept() : FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Requirements);
}
