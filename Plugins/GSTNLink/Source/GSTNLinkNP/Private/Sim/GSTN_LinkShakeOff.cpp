// Links, Network Prediction half.

#include "Sim/GSTN_LinkShakeOff.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkTickExtension.h"

uint8 FGSTN_LinkShakeOff::ReleaseReasonOf(const UGSTN_LinkTypeDefinition& Definition, uint8 Role, const UGSTN_LinkRegistry* Registry)
{
	const FGameplayTag& Reason = Definition.GetRoleDef(Role).ReleaseBreakReason;
	if (Reason.IsValid() && Registry)
	{
		const uint8 Index = Registry->GetReasons().IndexOf(Reason);
		if (Index != 0)
		{
			return Index;
		}
	}
	return static_cast<uint8>(EGSTN_LinkBreakReason::Release);
}

bool FGSTN_LinkShakeOff::ApplyReleaseKey(FGSTN_LinkFragment& Fragment, const UGSTN_LinkTypeDefinition& Definition, const UGSTN_LinkRegistry* Registry)
{
	const FGSTN_LinkState& Body = Fragment.Links.Body;
	if (!Body.IsActive() || Body.Role == 0)
	{
		return false;
	}
	return FGSTN_LinkTickExtension::MarkPendingBreak(Fragment, ReleaseReasonOf(Definition, Body.Role, Registry));
}
