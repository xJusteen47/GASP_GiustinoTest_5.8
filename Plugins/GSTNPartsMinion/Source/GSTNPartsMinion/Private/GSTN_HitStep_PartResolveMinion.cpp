// Bridge between parts and minions (plan P16).

#include "GSTN_HitStep_PartResolveMinion.h"

#include "Hits/GSTN_HitSteps.h"
#include "Pipeline/GSTN_PartsSnapshotExt.h"
#include "Rules/GSTN_CombatRules.h"

EGSTN_StepResult UGSTN_HitStep_PartResolveMinion::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	// A minion type without a part set: the rules' own steps already do everything
	if (!FGSTN_PartsSnapshotExt::FromView(View.Victim))
	{
		return EGSTN_StepResult::Continue;
	}

	bool bRulesHavePartResolve = false;
	bool bRulesHaveApplyDamage = false;
	bool bRulesHaveHealthWeight = false;
	if (View.Rules)
	{
		for (const TSubclassOf<UGSTN_HitResolutionStep>& StepClass : View.Rules->GetResolvedHitPipeline())
		{
			if (!StepClass || StepClass->IsChildOf(UGSTN_HitStep_PartResolveMinion::StaticClass()))
			{
				continue;
			}
			bRulesHavePartResolve |= StepClass->IsChildOf(UGSTN_HitStep_PartResolve::StaticClass());
			bRulesHaveApplyDamage |= StepClass->IsChildOf(UGSTN_HitStep_ApplyDamage::StaticClass());
			bRulesHaveHealthWeight |= StepClass->IsChildOf(UGSTN_HitStep_HealthWeight::StaticClass());
		}
	}
	if (bRulesHavePartResolve)
	{
		// Parts-aware rules: their PartResolve (validated before ComputeKnockback) already resolved the part on this same view
		return EGSTN_StepResult::Continue;
	}

	const EGSTN_StepResult Verdict = Super::Resolve_Implementation(View, Out);

	// The parent wrote the damage only past its own early-out (the same conditions ApplyDamage tests), and a Stop (ricochet) ends the
	// pipeline before ApplyDamage: compensate only when both steps write
	const bool bDamaging = View.Outcome.bHit && !View.bNoDamage && !View.Outcome.bBlocked && !View.Outcome.bParried;
	if (Verdict == EGSTN_StepResult::Continue && bDamaging)
	{
		if (bRulesHaveApplyDamage)
		{
			// ApplyDamage adds the hit's own damage after ComputeKnockback; the outcome must end at the part's share (ComputeKnockback reads the
			// hit, never Outcome.Damage, so the transient negative sum changes no knockback)
			const float HitDamage = View.Incoming.Hit.Damage;
			if (HitDamage > 0.f)
			{
				Out.DamageAdd -= HitDamage;
			}
			// ApplyDamage appends the extra effects again (minions ignore them, but they are counted): keep one copy
			Out.Effects.Reset();
			// Out.Requests keeps the part's damage request next to ApplyDamage's: the minion batch never reads requests (no ability system)
		}
		if (bRulesHaveHealthWeight)
		{
			// HealthWeight merged the progress weight before this slot; the parent's weight already contains it (EffectiveWeightScale)
			FGSTN_StepResult HealthWeight;
			GetDefault<UGSTN_HitStep_HealthWeight>()->Resolve(View, HealthWeight);
			if (HealthWeight.WeightScale > UE_KINDA_SMALL_NUMBER)
			{
				Out.WeightScale /= HealthWeight.WeightScale;
			}
		}
	}
	return Verdict;
}
