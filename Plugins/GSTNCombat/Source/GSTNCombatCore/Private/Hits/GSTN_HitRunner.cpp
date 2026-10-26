// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HitRunner.h"

#include "Rules/GSTN_CombatRules.h"

void FGSTN_HitPipelineRunner::MakeView(const UGSTN_CombatRules* Rules, const FGSTN_HitResolutionContext& Context, FGSTN_HitContextView& OutView)
{
	OutView.Incoming = Context.Incoming;
	OutView.Victim = Context.Victim;
	OutView.Reaction = Context.Reaction;
	OutView.HurtboxIndex = Context.HurtboxIndex;
	OutView.HurtState = Context.HurtState;
	OutView.ArmorThreshold = Context.ArmorThreshold;
	OutView.bNoDamage = Context.bNoDamage;
	OutView.Outcome = Context.Outcome;
	OutView.OutcomeExtensions = Context.OutcomeExtensions;
	OutView.Rules = Rules;
	OutView.HurtboxSet = Context.HurtboxSet;
}

void FGSTN_HitPipelineRunner::Merge(const FGSTN_StepResult& Result, FGSTN_HitResolutionContext& Context)
{
	FGSTN_HitReactionModifiers& Reaction = Context.Reaction;
	Reaction.WeightScale *= Result.WeightScale;
	Reaction.KnockbackScale *= Result.KnockbackScale;
	Reaction.HitstunScale *= Result.HitstunScale;
	Reaction.HitstopScale *= Result.HitstopScale;
	Reaction.StaggerScale *= Result.StaggerScale;
	if (Result.StaggerThreshold.IsSet())
	{
		Reaction.StaggerThreshold = Result.StaggerThreshold;
	}
	Reaction.MaxReaction = static_cast<EGSTN_HitReaction>(FMath::Min(static_cast<uint8>(Reaction.MaxReaction), static_cast<uint8>(Result.MaxReaction)));
	Reaction.bNoFlinch |= Result.bNoFlinch;
	Context.bNoDamage |= Result.bNoDamage;

	if (Result.Relationship.IsValid())
	{
		Context.Incoming.Relationship = Result.Relationship;
	}

	FGSTN_HitOutcome& Outcome = Context.Outcome;
	Outcome.bHit &= Result.bHit;
	Outcome.bBlocked |= Result.bBlocked;
	Outcome.bParried |= Result.bParried;
	Outcome.bClank |= Result.bClank;
	Outcome.Damage += Result.DamageAdd;
	if (Result.bHasKnockback)
	{
		Outcome.bHasKnockback = true;
		Outcome.Knockback = Result.Knockback;
	}
	Outcome.MeterGains.Append(Result.MeterGains);
	Outcome.Events.Append(Result.Events);
	Outcome.Effects.Append(Result.Effects);
	Outcome.Requests.Append(Result.Requests);
	Context.OutcomeExtensions.Append(Result.OutcomeExtensions);
	Context.OutgoingEntries.Append(Result.OutgoingEntries);
}

FGSTN_HitRunResult FGSTN_HitPipelineRunner::Run(const UGSTN_CombatRules& Rules, FGSTN_HitResolutionContext& Context)
{
	FGSTN_HitRunResult Run;
	const TArray<TSubclassOf<UGSTN_HitResolutionStep>>& Steps = Rules.GetResolvedHitPipeline();
	if (Steps.IsEmpty())
	{
		return Run;
	}

	FGSTN_HitContextView View;
	MakeView(&Rules, Context, View);

	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		const UGSTN_HitResolutionStep* Step = Steps[Index] ? GetDefault<UGSTN_HitResolutionStep>(Steps[Index]) : nullptr;
		if (!Step)
		{
			continue;
		}

		FGSTN_StepResult Result;
		const EGSTN_StepResult Verdict = Step->Resolve(View, Result);
		++Run.StepsRun;

		Merge(Result, Context);
		if (Verdict == EGSTN_StepResult::Stop)
		{
			Run.bStopped = true;
			Run.StoppedAtStep = Index;
			break;
		}
		MakeView(&Rules, Context, View);
	}
	return Run;
}

void FGSTN_HitPipelineRunner::AdvanceVictim(const UGSTN_CombatRules& Rules, const FGSTN_HitOutcome& Outcome, FGSTN_HitVictimView& Victim)
{
	if (!Outcome.bHit || Outcome.Damage <= 0.f || Rules.MaxHealth <= 0.f)
	{
		return;
	}

	if (Rules.DamageMode == EGSTN_DamageMode::Health)
	{
		Victim.HealthFraction = FMath::Clamp(Victim.HealthFraction - Outcome.Damage / Rules.MaxHealth, 0.f, 1.f);
		return;
	}

	double Percent = static_cast<double>(Victim.ReactionState.GetDamagePercent()) + Outcome.Damage;
	if (Rules.DamageMode == EGSTN_DamageMode::PercentCapped)
	{
		Percent = FMath::Min(Percent, static_cast<double>(Rules.MaxHealth));
	}
	const FGSTN_VictimReactionState& Old = Victim.ReactionState;
	Victim.ReactionState = FGSTN_VictimReactionState::Make(Percent, Old.GetWeightScale(), Old.Flags, Old.EffectiveThresholdQ, Old.ExtMask);
}
