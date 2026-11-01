#include "GSTN_ExtTestCarryRule.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNExtTestLink)

namespace GSTNExtTestLinkTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Type_Carry, "Combat.Link.Type.GSTNExtTestCarry", "Test only: a carry link type added by the extension test plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Carrier, "Combat.Link.Role.GSTNExtTestCarrier", "Test only: the carry's initiator, keeps acting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Carried, "Combat.Link.Role.GSTNExtTestCarried", "Test only: the carry's target, parked on the carrier");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Target_Carry, "Interact.Target.GSTNExtTestCarry", "Test only: the carry's interact meaning, a second meaning of the link probe");
}

float UGSTN_ExtTestCarryRule::GetCapacity(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_ExtTestCarryKit* Kit = Context.Initiator.FindKitExtension<FGSTN_ExtTestCarryKit>();
	return Kit ? Kit->MaxCarryWeight : MaxCarryWeight;
}

FGSTN_LinkRuleResult UGSTN_ExtTestCarryRule::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}
	return Context.Target.EffectiveWeight <= GetCapacity(Context) ? FGSTN_LinkRuleResult::Accept() : FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Requirements);
}

uint8 UGSTN_ExtTestCarryRule::ComputePayload_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const float Capacity = FMath::Max(GetCapacity(Context), 1.f);
	return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(255.f * Context.Target.EffectiveWeight / Capacity), 0, 255));
}
