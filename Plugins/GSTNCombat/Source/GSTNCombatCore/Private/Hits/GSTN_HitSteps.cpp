// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HitSteps.h"

#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Rules/GSTN_CombatRules.h"

EGSTN_StepResult UGSTN_HitResolutionStep::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_TeamFilter::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	FGameplayTag Relationship = View.Incoming.Relationship;
	if (!Relationship.IsValid())
	{
		FGSTN_RelationshipQuery Query;
		Query.AttackerEntityId = View.Incoming.AttackerEntityId;
		Query.AttackerTeamId = View.Incoming.AttackerTeamId;
		Query.VictimEntityId = View.Victim.EntityId;
		Query.VictimTeamId = View.Victim.TeamId;
		Relationship = View.Rules ? View.Rules->ResolveRelationship(Query) : UGSTN_RelationshipResolver::DefaultRule(Query);
		Out.Relationship = Relationship;
	}

	// Without rules nothing says who may be hit: the step is the identity
	if (View.Rules && !View.Rules->IsRelationshipHittable(Relationship))
	{
		Out.bHit = false;
		return EGSTN_StepResult::Stop;
	}
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_HurtState::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	switch (View.HurtState)
	{
	case EGSTN_HurtState::Intangible:
		Out.bHit = false;
		return EGSTN_StepResult::Stop;
	case EGSTN_HurtState::Invincible:
		// The hit registers (hitstop on both sides, confirm) but nothing reaches the victim
		Out.bNoDamage = true;
		Out.bNoFlinch = true;
		break;
	default:
		break;
	}
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_HurtboxReaction::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	const UGSTN_HurtboxSetDefinition* Set = View.HurtboxSet;
	const FGSTN_Hurtbox* Box = Set && Set->Hurtboxes.IsValidIndex(View.HurtboxIndex) ? &Set->Hurtboxes[View.HurtboxIndex] : nullptr;

	if (Box)
	{
		const FGSTN_HitReactionModifiers& Scale = Box->ReactionScale;
		Out.WeightScale = Scale.WeightScale;
		Out.KnockbackScale = Scale.KnockbackScale;
		Out.HitstunScale = Scale.HitstunScale;
		Out.HitstopScale = Scale.HitstopScale;
		Out.StaggerScale = Scale.StaggerScale;
		Out.bNoFlinch = Scale.bNoFlinch;
		Out.MaxReaction = Scale.MaxReaction;
	}
	if (Set)
	{
		Out.MaxReaction = static_cast<EGSTN_HitReaction>(FMath::Min(static_cast<uint8>(Out.MaxReaction), static_cast<uint8>(Set->MaxReaction)));
	}

	// The first threshold that is set wins: the hurtbox's, the entity's, the rules'
	FGSTN_StaggerThreshold Threshold;
	if (Box && Box->ReactionScale.StaggerThreshold.IsSet())
	{
		Threshold = Box->ReactionScale.StaggerThreshold;
	}
	else if (Set && Set->StaggerThreshold.IsSet())
	{
		Threshold = Set->StaggerThreshold;
	}
	else if (View.Rules)
	{
		Threshold.Value = View.Rules->StaggerThreshold;
	}

	// An Armor window is a temporary threshold: the larger of the two applies while it is open
	if (View.HurtState == EGSTN_HurtState::Armor)
	{
		Threshold = FGSTN_StaggerThreshold::Max(Threshold, View.ArmorThreshold);
	}
	Out.StaggerThreshold = Threshold;
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_HealthWeight::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Rules)
	{
		return EGSTN_StepResult::Continue;
	}

	const FGSTN_BakedCurve* Curve = nullptr;
	if (View.HurtboxSet && View.HurtboxSet->WeightCurve.IsAuthored())
	{
		Curve = &View.HurtboxSet->WeightCurve;
	}
	else if (View.Rules->DefaultWeightCurve.IsAuthored())
	{
		Curve = &View.Rules->DefaultWeightCurve;
	}

	if (Curve)
	{
		Out.WeightScale = Curve->Evaluate(View.Rules->GetDamageProgress(View.Victim));
	}
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_ComputeKnockback::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	const UGSTN_KnockbackModel* Model = View.Rules ? View.Rules->KnockbackModel.Get() : nullptr;
	if (!Model)
	{
		return EGSTN_StepResult::Continue;
	}

	const FGSTN_ResolvedHit& Incoming = View.Incoming;

	FGSTN_KnockbackInput Input;
	Input.Hit = Incoming.Hit;
	Input.Up = Incoming.Up;
	Input.Weight = View.Victim.Weight;
	Input.DamagePercent = View.Victim.ReactionState.GetDamagePercent();
	Input.HealthFraction = View.Victim.HealthFraction;
	Input.Reaction = View.Reaction;
	if (View.Victim.ReactionState.HasFlag(GSTNVictimReactionFlags::Tumble))
	{
		Input.Reaction.KnockbackScale *= View.Rules->AirborneJuggleScale;
	}

	// Sakurai3D picks the angle from K, and K does not depend on the direction: read it with no direction first
	double Knockback = 0.0;
	if (Incoming.Hit.SpecialAngle == EGSTN_SpecialAngle::Sakurai3D)
	{
		Input.LaunchDirection = FVector::ZeroVector;
		Knockback = Model->Compute(Input).Knockback;
	}

	Input.LaunchDirection = UGSTN_HitMathUtils::LaunchDirection(Incoming.Hit.YawSource, Incoming.Hit.AngleDeg, Incoming.Hit.SpecialAngle, Incoming.Up,
		Incoming.AttackerFacing, Incoming.HitboxToVictim, Incoming.HitboxVelocity, Incoming.bVictimGrounded, static_cast<float>(Knockback), View.Rules->Sakurai);

	Out.bHasKnockback = true;
	Out.Knockback = Model->Compute(Input);
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_ApplyDamage::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit || View.bNoDamage || View.Outcome.bBlocked || View.Outcome.bParried)
	{
		return EGSTN_StepResult::Continue;
	}

	const FGSTN_HitData& Hit = View.Incoming.Hit;
	if (Hit.Damage > 0.f)
	{
		Out.DamageAdd = Hit.Damage;

		FGSTN_HitRequest& Request = Out.Requests.AddDefaulted_GetRef();
		Request.Kind = EGSTN_HitRequestKind::Damage;
		Request.Target = EGSTN_HitRequestTarget::Victim;
		Request.Effect = View.Rules ? View.Rules->DamageEffect : TSoftClassPtr<UObject>();
		Request.Magnitude = Hit.Damage;
	}
	Out.Effects.Append(Hit.ExtraEffects);
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_Hitstop::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Outcome.bHit || !View.Outcome.bHasKnockback)
	{
		return EGSTN_StepResult::Continue;
	}

	const TSoftClassPtr<UObject> Effect = View.Rules ? View.Rules->FreezeEffect : TSoftClassPtr<UObject>();
	const FGSTN_KnockbackResult& Knockback = View.Outcome.Knockback;
	if (Knockback.HitstopFrames > 0)
	{
		FGSTN_HitRequest& Request = Out.Requests.AddDefaulted_GetRef();
		Request.Kind = EGSTN_HitRequestKind::Freeze;
		Request.Target = EGSTN_HitRequestTarget::Victim;
		Request.Effect = Effect;
		Request.Magnitude = static_cast<float>(Knockback.HitstopFrames);
	}
	if (Knockback.AttackerHitstopFrames > 0)
	{
		FGSTN_HitRequest& Request = Out.Requests.AddDefaulted_GetRef();
		Request.Kind = EGSTN_HitRequestKind::Freeze;
		Request.Target = EGSTN_HitRequestTarget::Attacker;
		Request.Effect = Effect;
		Request.Magnitude = static_cast<float>(Knockback.AttackerHitstopFrames);
	}
	return EGSTN_StepResult::Continue;
}

EGSTN_StepResult UGSTN_HitStep_MeterGain::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	if (!View.Rules || !View.Outcome.bHit || View.Rules->MeterGainPerDamage <= 0.f)
	{
		return EGSTN_StepResult::Continue;
	}

	// The damage the ApplyDamage step wrote: a hit that did no damage fills no meter
	const float Amount = View.Outcome.Damage * View.Rules->MeterGainPerDamage;
	if (Amount > 0.f)
	{
		FGSTN_MeterGain& Gain = Out.MeterGains.AddDefaulted_GetRef();
		Gain.Meter = View.Rules->MeterGainMeter;
		Gain.Amount = Amount;

		FGSTN_HitRequest& Request = Out.Requests.AddDefaulted_GetRef();
		Request.Kind = EGSTN_HitRequestKind::MeterGain;
		Request.Target = EGSTN_HitRequestTarget::Attacker;
		Request.Effect = View.Rules->MeterGainEffect;
		Request.Magnitude = Amount;
	}
	return EGSTN_StepResult::Continue;
}
