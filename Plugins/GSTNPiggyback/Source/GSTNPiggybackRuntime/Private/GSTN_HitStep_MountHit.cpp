// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_HitStep_MountHit.h"

#include "GSTN_LinkRegistry.h"
#include "GSTN_MountLinkType.h"
#include "GSTN_MountUtils.h"
#include "GSTN_PiggybackTags.h"
#include "Pipeline/GSTN_HitStep_LinkBreakOnHit.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "State/GSTN_LinkState.h"

namespace
{
	/** The role an extension holds in an active mount link, or None */
	uint8 MountRoleOf(const FGSTN_LinkSnapshotExt* Ext, const UGSTN_LinkRegistry& Registry, const UGSTN_LinkTypeDefinition* Mount)
	{
		if (!Ext || !Mount || Ext->BodyPhase != static_cast<uint8>(EGSTN_LinkPhase::Active) || Registry.GetDefinition(Ext->BodyTypeIndex) != Mount)
		{
			return GSTNMountRole::None;
		}
		return Ext->BodyRole == GSTNMountRole::Rider || Ext->BodyRole == GSTNMountRole::Carrier ? Ext->BodyRole : GSTNMountRole::None;
	}
}

const FGSTN_MountRules& UGSTN_HitStep_MountHit::FindRules(const UGSTN_CombatRules* Rules)
{
	static const FGSTN_MountRules Defaults;
	if (Rules)
	{
		for (const FInstancedStruct& Extension : Rules->RulesExtensions)
		{
			if (const FGSTN_MountRules* Found = Extension.GetPtr<FGSTN_MountRules>())
			{
				return *Found;
			}
		}
	}
	return Defaults;
}

bool UGSTN_HitStep_MountHit::IsInFrontArc(const FVector& HitboxToVictim, float FacingYaw, float ArcDeg)
{
	if (ArcDeg <= 0.f)
	{
		return false;
	}
	// The hit comes from where the hitbox is: the reverse of its travel, on the movement plane
	const FVector2D From = FVector2D(-HitboxToVictim.X, -HitboxToVictim.Y).GetSafeNormal();
	if (From.IsNearlyZero())
	{
		return false;
	}
	const float Rad = FMath::DegreesToRadians(FacingYaw);
	const FVector2D Facing(FMath::Cos(Rad), FMath::Sin(Rad));
	const float HalfCos = FMath::Cos(FMath::DegreesToRadians(FMath::Min(ArcDeg, 360.f) * 0.5f));
	return FVector2D::DotProduct(From, Facing) >= HalfCos - KINDA_SMALL_NUMBER;
}

FGSTN_MountHitDecision UGSTN_HitStep_MountHit::Evaluate(const FGSTN_MountHitInput& Input, const FGSTN_MountRules& Rules)
{
	FGSTN_MountHitDecision Decision;

	if (Input.VictimRole != GSTNMountRole::None && Input.bFromPartner && Input.LinkFilter != EGSTN_LinkFilter::LinkedPartnerOnly)
	{
		Decision.bDrop = true;
		return Decision;
	}

	if (Input.VictimRole == GSTNMountRole::Rider && !Input.bFromPartner && Input.bInCoverArc)
	{
		Decision.bDrop = true;
		Decision.bCovered = true;
		return Decision;
	}

	if (Input.VictimRole == GSTNMountRole::Rider && !Input.bFromPartner)
	{
		if (Input.bAboveBreak)
		{
			Decision.BreakReason = GSTNPiggybackTags::BreakReason_RiderHit;
		}
		else if (Input.bHasKnockback)
		{
			FGSTN_KnockbackResult Scaled = Input.Knockback;
			Scaled.Knockback *= Rules.RiderKnockbackScale;
			Scaled.LaunchVelocity *= Rules.RiderKnockbackScale;
			Scaled.HitstunFrames = FMath::RoundToInt32(static_cast<float>(Scaled.HitstunFrames) * Rules.RiderHitstunScale);
			if (Scaled.Knockback <= 0.f)
			{
				// Not launched: it flinches where it sits
				Scaled.Reaction = EGSTN_HitReaction::StaggerInPlace;
				Scaled.bTumble = false;
			}
			Decision.bScaled = true;
			Decision.Knockback = Scaled;
		}
	}
	else if (Input.VictimRole == GSTNMountRole::Carrier && !Input.bFromPartner && Input.bAboveBreak)
	{
		Decision.BreakReason = GSTNPiggybackTags::BreakReason_CarrierHit;
	}

	if (Input.AttackerRole != GSTNMountRole::None && Input.AttackerPartnerId != INDEX_NONE && Input.AttackerPartnerId != Input.VictimEntityId)
	{
		switch (Rules.DefaultCredit)
		{
		case EGSTN_PairCredit::InitiatorAndOwner:
			Decision.Credit.PartnerEntityId = Input.AttackerPartnerId;
			Decision.Credit.PartnerShare = 0;
			break;
		case EGSTN_PairCredit::Split:
			Decision.Credit.PartnerEntityId = Input.AttackerPartnerId;
			Decision.Credit.PartnerShare = UGSTN_MountUtils::MakePartnerShare(0.5);
			break;
		default:
			break;
		}
	}
	return Decision;
}

EGSTN_StepResult UGSTN_HitStep_MountHit::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit || View.Outcome.bBlocked || View.Outcome.bParried)
	{
		return EGSTN_StepResult::Continue;
	}
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const UGSTN_LinkTypeDefinition* Mount = FGSTN_MountLinkType::Get();
	if (!Registry || !Mount)
	{
		return EGSTN_StepResult::Continue;
	}

	const FGSTN_LinkSnapshotExt* VictimExt = FGSTN_LinkSnapshotExt::FromVictim(View.Victim);
	const FGSTN_LinkSnapshotExt* AttackerExt = FGSTN_LinkSnapshotExt::FromAttacker(View.Incoming);
	const FGSTN_MountRules& Rules = FindRules(View.Rules);

	FGSTN_MountHitInput Input;
	Input.VictimRole = MountRoleOf(VictimExt, *Registry, Mount);
	Input.AttackerRole = MountRoleOf(AttackerExt, *Registry, Mount);
	if (Input.VictimRole == GSTNMountRole::None && Input.AttackerRole == GSTNMountRole::None)
	{
		return EGSTN_StepResult::Continue;
	}
	Input.VictimEntityId = View.Victim.EntityId;
	Input.AttackerPartnerId = Input.AttackerRole != GSTNMountRole::None ? AttackerExt->PartnerId : INDEX_NONE;
	Input.bFromPartner = Input.VictimRole != GSTNMountRole::None && VictimExt->PartnerId != INDEX_NONE
		&& VictimExt->PartnerId == View.Incoming.AttackerEntityId;
	Input.LinkFilter = View.Incoming.Hit.LinkFilter;
	Input.bHasKnockback = View.Outcome.bHasKnockback;
	Input.Knockback = View.Outcome.Knockback;
	if (Input.VictimRole != GSTNMountRole::None)
	{
		Input.bAboveBreak = UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak(*Mount, Input.VictimRole, Input.bFromPartner,
			Input.bHasKnockback ? Input.Knockback.Knockback : 0.f);
	}
	Input.bInCoverArc = Input.VictimRole == GSTNMountRole::Rider
		&& IsInFrontArc(View.Incoming.HitboxToVictim, View.Victim.FacingYaw, Rules.CarrierCoverFrontDeg);

	const FGSTN_MountHitDecision Decision = Evaluate(Input, Rules);
	if (Decision.bDrop)
	{
		Out.bHit = false;
		return EGSTN_StepResult::Stop;
	}
	if (Decision.bScaled)
	{
		Out.bHasKnockback = true;
		Out.Knockback = Decision.Knockback;
	}
	if (Decision.BreakReason.IsValid())
	{
		const uint8 ReasonIndex = Registry->GetReasons().IndexOf(Decision.BreakReason);
		FGSTN_LinkBreakOutcome Break;
		Break.Channel = EGSTN_LinkChannel::Body;
		Break.Serial = VictimExt->BodySerial;
		Break.Reason = ReasonIndex != 0 ? ReasonIndex : static_cast<uint8>(EGSTN_LinkBreakReason::Hit);
		Out.OutcomeExtensions.Add(FInstancedStruct::Make(Break));
	}
	if (Decision.Credit.IsSet())
	{
		Out.Credit = Decision.Credit;
	}
	return EGSTN_StepResult::Continue;
}
