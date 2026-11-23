// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_LinkRule_Mount.h"

#include "GSTN_CombatCoreTags.h"
#include "GSTN_LinkTags.h"
#include "GSTN_MountUtils.h"

FGSTN_MountEligibilityInput UGSTN_LinkRule_Mount::MakeEligibilityInput(const FGSTN_LinkRequestContext& Context)
{
	const FGSTN_MountKitExtension* CarrierKit = Context.Target.FindKitExtension<FGSTN_MountKitExtension>();
	const FGSTN_MountKitExtension* RiderKit = Context.Initiator.FindKitExtension<FGSTN_MountKitExtension>();

	FGSTN_MountEligibilityInput Input;
	Input.Frame = Context.Frame;

	Input.bCarrierCanBeMounted = CarrierKit && CarrierKit->Carrier.bCanBeMounted;
	Input.bCarrierLinked = Context.Target.Links.Body.IsLinked();
	Input.bCarrierInHitstun = Context.Target.Tags.HasTag(GSTNCombatTags::State_Hitstun);
	Input.bCarrierBusyOrLocked = Context.Target.Tags.HasTag(GSTNLinkTags::Lock_Link);
	Input.CarrierMaxCarryWeight = CarrierKit ? CarrierKit->Carrier.CarryCapacity : 0.0;
	// Range is the hitbox's and the offer's reach (the context has no position): the check is kept, with the distance at zero
	Input.MountMaxDistanceCm = CarrierKit ? CarrierKit->Carrier.MountMaxDistance : 0.0;
	Input.DistanceCm = 0.0;

	Input.bRiderCanRide = RiderKit && RiderKit->Rider.bCanRide;
	// The LinkRequest step already removed the rider's own Pending request for this carrier: any Body link left is another one
	Input.bRiderLinked = Context.Initiator.Links.Body.IsLinked();
	Input.RiderWeight = RiderKit ? RiderKit->Rider.Weight : 0.0;
	Input.RiderCooldownEndFrame = Context.Initiator.CooldownEndFrame;
	return Input;
}

EGSTN_LinkRefusal UGSTN_LinkRule_Mount::ToRefusal(EGSTN_MountVerdict Verdict)
{
	switch (Verdict)
	{
	case EGSTN_MountVerdict::Accept:
		return EGSTN_LinkRefusal::None;
	case EGSTN_MountVerdict::AlreadyLinked:
		return EGSTN_LinkRefusal::TargetBusy;
	case EGSTN_MountVerdict::CarrierHitstun:
		return EGSTN_LinkRefusal::RequestRequirements;
	case EGSTN_MountVerdict::CarrierBusyLock:
		return EGSTN_LinkRefusal::TargetLocked;
	case EGSTN_MountVerdict::WeightTooHigh:
		return EGSTN_LinkRefusal::TooHeavy;
	case EGSTN_MountVerdict::Cooldown:
		return EGSTN_LinkRefusal::Cooldown;
	case EGSTN_MountVerdict::NotAllowedKit:
	case EGSTN_MountVerdict::RiderIneligible:
	case EGSTN_MountVerdict::OutOfRange:
	default:
		return EGSTN_LinkRefusal::Requirements;
	}
}

FGSTN_LinkRuleResult UGSTN_LinkRule_Mount::Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_LinkRuleResult Common = CheckCommon(Context);
	if (!Common.bAccepted)
	{
		return Common;
	}
	const EGSTN_MountVerdict Verdict = UGSTN_MountUtils::EvaluateEligibility(MakeEligibilityInput(Context));
	return Verdict == EGSTN_MountVerdict::Accept ? FGSTN_LinkRuleResult::Accept() : FGSTN_LinkRuleResult::Refuse(ToRefusal(Verdict));
}

uint8 UGSTN_LinkRule_Mount::ComputePayload_Implementation(const FGSTN_LinkRequestContext& Context) const
{
	const FGSTN_MountEligibilityInput Input = MakeEligibilityInput(Context);
	return UGSTN_MountUtils::ComputeCarryRatio(Input.RiderWeight, Input.CarrierMaxCarryWeight);
}
