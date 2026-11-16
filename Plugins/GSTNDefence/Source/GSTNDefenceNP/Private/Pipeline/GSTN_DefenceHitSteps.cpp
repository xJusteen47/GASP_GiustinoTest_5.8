// Defence, Network Prediction half.

#include "Pipeline/GSTN_HitStep_Parry.h"
#include "Pipeline/GSTN_HitStep_Shield.h"
#include "Pipeline/GSTN_DefenceHitOutcome.h"
#include "Data/GSTN_DefenceRules.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Math/GSTN_DefenceMath.h"
#include "Rules/GSTN_CombatRules.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceInboxKinds.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"

bool FGSTN_DefenceStepUtils::Find(const FGSTN_HitVictimView& Victim, const FGSTN_DefenceSnapshotExt*& OutExt, const UGSTN_DefenceRules*& OutRules)
{
	OutExt = FGSTN_DefenceSnapshotExt::FromView(Victim);
	OutRules = OutExt ? FGSTN_DefenceAttach::RulesAt(OutExt->RulesIndex) : nullptr;
	return OutExt && OutRules;
}

bool FGSTN_DefenceStepUtils::IsDefendedRelationship(FGameplayTag Relationship)
{
	return Relationship == GSTNCombatHitTags::Relationship_Enemy.GetTag() || Relationship == GSTNCombatHitTags::Relationship_Neutral.GetTag();
}

bool FGSTN_DefenceStepUtils::IsProjectileHitType(FGameplayTag HitType)
{
	return HitType.IsValid() && HitType.MatchesTag(GSTNCombatHitTags::HitType_Projectile.GetTag());
}

namespace
{
	/** The move window covers the hurtbox: no selector (mask 0) covers any, else bit i of the mask (bit 15 says a selector exists) */
	bool MoveCoversHurtbox(uint16 Mask, int32 HurtboxIndex)
	{
		if (Mask == 0)
		{
			return true;
		}
		return HurtboxIndex >= 0 && HurtboxIndex < 15 && (Mask & (1u << HurtboxIndex)) != 0;
	}

	/** A knockback result that stops the victim and the attacker for these frames and staggers nobody */
	FGSTN_KnockbackResult MakeHitstopOnly(int32 VictimFrames, int32 AttackerFrames)
	{
		FGSTN_KnockbackResult Result;
		Result.bStaggered = false;
		Result.Reaction = EGSTN_HitReaction::StaggerInPlace;
		Result.HitstopFrames = FMath::Max(VictimFrames, 0);
		Result.AttackerHitstopFrames = FMath::Max(AttackerFrames, 0);
		return Result;
	}

	/** The hitstop the knockback model gives this hit (victim, attacker), as ComputeKnockback would, without the launch direction (hitstop does not depend on it) */
	void ModelHitstop(const FGSTN_HitContextView& View, int32& OutVictim, int32& OutAttacker)
	{
		OutVictim = 0;
		OutAttacker = 0;
		const UGSTN_KnockbackModel* Model = View.Rules ? View.Rules->KnockbackModel.Get() : nullptr;
		if (!Model)
		{
			return;
		}
		FGSTN_KnockbackInput Input;
		Input.Hit = View.Incoming.Hit;
		Input.Up = View.Incoming.Up;
		Input.Weight = View.Victim.Weight;
		Input.DamagePercent = View.Victim.ReactionState.GetDamagePercent();
		Input.HealthFraction = View.Victim.HealthFraction;
		Input.Reaction = View.Reaction;
		Input.LaunchDirection = FVector::ZeroVector;
		const FGSTN_KnockbackResult Result = Model->Compute(Input);
		OutVictim = Result.HitstopFrames;
		OutAttacker = Result.AttackerHitstopFrames;
	}
}

EGSTN_StepResult UGSTN_HitStep_Parry::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	const FGSTN_DefenceSnapshotExt* Ext = nullptr;
	const UGSTN_DefenceRules* Rules = nullptr;
	if (!View.Outcome.bHit || !FGSTN_DefenceStepUtils::Find(View.Victim, Ext, Rules) || !FGSTN_DefenceStepUtils::IsDefendedRelationship(View.Incoming.Relationship))
	{
		return EGSTN_StepResult::Continue;
	}
	const FGSTN_HitData& Hit = View.Incoming.Hit;
	const int32 HitFrame = View.Incoming.Frame;
	if (HitFrame == INDEX_NONE)
	{
		return EGSTN_StepResult::Continue;
	}

	FGSTN_ParryContext Parry;
	Parry.HitFrame = HitFrame;
	Parry.bHitParryable = Hit.bParryable;
	Parry.bVictimImmune = Ext->Has(GSTNDefenceSnapshotFlags::ImmuneParry);
	Parry.bGrabLikeHit = UGSTN_DefenceMath::IsGrabLikeHitType(Hit.HitType);
	Parry.bProjectile = FGSTN_DefenceStepUtils::IsProjectileHitType(Hit.HitType);
	Parry.bHitOnBubble = GSTNHurtboxIndex::IsExtra(View.HurtboxIndex);
	Parry.bShieldAvailable = Ext->IsShieldAvailable() && Ext->Has(GSTNDefenceSnapshotFlags::Bubble);
	Parry.ShieldPressFrame = Ext->ShieldPressFrame;
	Parry.ShieldReleaseFrame = Ext->ShieldReleaseFrame;
	Parry.MoveStartFrame = Ext->MoveParryStartFrame;
	Parry.MoveEndFrame = Ext->MoveParryEndFrame;
	Parry.MoveProfileIndex = Ext->MoveParryProfile & ~FGSTN_DefenceStepUtils::CloseOnParryBit;
	Parry.bMoveCoversHurtbox = MoveCoversHurtbox(Ext->MoveParryHurtboxMask, View.HurtboxIndex);
	const FGSTN_ParryDecision Decision = UGSTN_DefenceMath::ResolveParry(Parry, Rules);
	if (!Decision.IsParry() || !Rules->ParryProfiles.IsValidIndex(Decision.ProfileIndex))
	{
		// Not parried: the hit goes on, and the applier still records what it allows the victim's DI and SDI (F8: Result None, its hit frame)
		FGSTN_DefenceHitOutcome Passed;
		Passed.HitFrame = HitFrame;
		Out.OutcomeExtensions.Add(FInstancedStruct::Make(Passed));
		return EGSTN_StepResult::Continue;
	}
	const FGSTN_ParryProfile& Profile = Rules->ParryProfiles[Decision.ProfileIndex];

	// Nullified: no damage, no launch, no hitstun; both sides stand still for the profile's hitstop
	Out.bParried = true;
	Out.bNoDamage = true;
	Out.bHasKnockback = true;
	Out.Knockback = MakeHitstopOnly(Profile.HitstopFrames, Profile.HitstopFrames);

	FGSTN_DefenceHitOutcome Outcome;
	Outcome.Result = EGSTN_DefenceHitResult::Parried;
	Outcome.Source = Decision.Source;
	Outcome.ProfileIndex = static_cast<uint8>(Decision.ProfileIndex);
	Outcome.HitFrame = HitFrame;
	Outcome.HitstopFrames = FMath::Max(Profile.HitstopFrames, 0);
	if (Decision.Source == EGSTN_ParrySource::Shield)
	{
		Outcome.DefenderRecoveryFrames = FMath::Max(Profile.DefenderRecoveryFrames, 0);
	}
	else
	{
		const bool bCloseOnParry = (Ext->MoveParryProfile & FGSTN_DefenceStepUtils::CloseOnParryBit) != 0;
		Outcome.MoveWindowEndFrame = UGSTN_DefenceMath::MoveWindowEndAfterParry(bCloseOnParry, HitFrame, Ext->MoveParryEndFrame);
	}
	Out.OutcomeExtensions.Add(FInstancedStruct::Make(Outcome));

	// The attacker's punish, by reverse delivery (posted by the victim step once the hit stands; the attacker's own run ignores it)
	FGSTN_DefencePunishEntry Punish;
	Punish.Kind = GSTNPunishKinds::Parried;
	Punish.HitFrame = HitFrame;
	Punish.HitstopFrames = Outcome.HitstopFrames;
	Punish.StunFrames = FMath::Max(UGSTN_DefenceMath::AttackerStunFramesOf(Profile, Hit.HitType), 0);
	if (Punish.StunFrames > 0)
	{
		Out.OutgoingEntries.Add(FGSTN_DefencePunishHandler::MakeEntry(Punish));
	}
	return EGSTN_StepResult::Stop;
}

EGSTN_StepResult UGSTN_HitStep_Shield::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	const FGSTN_DefenceSnapshotExt* Ext = nullptr;
	const UGSTN_DefenceRules* Rules = nullptr;
	if (!View.Outcome.bHit || View.Outcome.bParried || !FGSTN_DefenceStepUtils::Find(View.Victim, Ext, Rules))
	{
		return EGSTN_StepResult::Continue;
	}
	// The mode (or the kit) has no shield: nothing of it exists, the step is skipped
	if (!Ext->IsShieldAvailable() || !FGSTN_DefenceStepUtils::IsDefendedRelationship(View.Incoming.Relationship))
	{
		return EGSTN_StepResult::Continue;
	}
	// Only a hit that landed on the bubble is blocked; a body hurtbox of a shielding victim takes an ordinary hit, no event
	if (!GSTNHurtboxIndex::IsExtra(View.HurtboxIndex) || !Ext->Has(GSTNDefenceSnapshotFlags::Bubble) || !Ext->Has(GSTNDefenceSnapshotFlags::Blocks))
	{
		return EGSTN_StepResult::Continue;
	}
	const FGSTN_HitData& Hit = View.Incoming.Hit;
	const FGSTN_ShieldRules& Shield = Rules->Shield;
	const int32 HitFrame = View.Incoming.Frame;

	const double ShieldDamage = UGSTN_DefenceMath::ShieldDamageOf(Hit.ShieldDamage, Hit.Damage, Shield, Ext->GetShieldDamageMultiplier(),
		UGSTN_DefenceMath::RelationshipScaleOf(Shield, View.Incoming.Relationship));
	const double After = Ext->GetShieldValue() - FMath::Max(ShieldDamage, 0.0);

	int32 VictimHitstop = 0;
	int32 AttackerHitstop = 0;
	ModelHitstop(View, VictimHitstop, AttackerHitstop);
	VictimHitstop = FMath::Max(FMath::RoundToInt(VictimHitstop * Shield.ShieldHitstopMultiplier), 0);
	AttackerHitstop = FMath::Max(FMath::RoundToInt(AttackerHitstop * Shield.ShieldHitstopMultiplier), 0);

	Out.bBlocked = true;
	Out.bNoDamage = true;
	Out.bHasKnockback = true;
	Out.Knockback = MakeHitstopOnly(VictimHitstop, AttackerHitstop);

	FGSTN_DefenceHitOutcome Outcome;
	Outcome.HitFrame = HitFrame;
	Outcome.HitstopFrames = VictimHitstop;
	if (After <= 0.0)
	{
		Outcome.Result = EGSTN_DefenceHitResult::Broken;
		Outcome.ShieldValueAfterQ = 0;
		Outcome.StunFrames = FMath::Max(Shield.BreakStunFrames, 0);
		Outcome.Velocity = View.Incoming.Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector) * FMath::Max(Shield.BreakLaunch, 0.f);
	}
	else
	{
		Outcome.Result = EGSTN_DefenceHitResult::Blocked;
		Outcome.ShieldValueAfterQ = FMath::Max<int16>(FGSTN_DefenceFragment::QuantizeValue(After), int16(1));
		Outcome.StunFrames = UGSTN_DefenceMath::ShieldStunFrames(Hit.Damage, Shield);
		// Along the hit on the victim's ground plane (the attacker is not pushed in this version)
		const FVector Up = View.Incoming.Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector Along = FVector::VectorPlaneProject(View.Incoming.HitboxToVictim, Up).GetSafeNormal();
		Outcome.Velocity = Along * UGSTN_DefenceMath::PushbackSpeedOf(Hit.Damage, Shield);
		Outcome.PushbackFrames = Along.IsNearlyZero() ? 0 : FMath::Clamp(Shield.PushbackFrames, 0, 255);
	}
	Out.OutcomeExtensions.Add(FInstancedStruct::Make(Outcome));

	// The attacker's meter, as an ordinary hit of that damage would give it
	if (View.Rules && View.Rules->MeterGainPerDamage > 0.f && Hit.Damage > 0.f)
	{
		const float Amount = Hit.Damage * View.Rules->MeterGainPerDamage;
		FGSTN_MeterGain& Gain = Out.MeterGains.AddDefaulted_GetRef();
		Gain.Meter = View.Rules->MeterGainMeter;
		Gain.Amount = Amount;
		FGSTN_HitRequest& Request = Out.Requests.AddDefaulted_GetRef();
		Request.Kind = EGSTN_HitRequestKind::MeterGain;
		Request.Target = EGSTN_HitRequestTarget::Attacker;
		Request.Effect = View.Rules->MeterGainEffect;
		Request.Magnitude = Amount;
	}
	return EGSTN_StepResult::Stop;
}
