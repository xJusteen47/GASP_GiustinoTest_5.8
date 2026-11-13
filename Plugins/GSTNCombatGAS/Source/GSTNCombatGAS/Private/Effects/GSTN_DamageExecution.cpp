// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Effects/GSTN_DamageExecution.h"
#include "Attributes/GSTN_HealthAttributeSet.h"
#include "GSTN_CombatTags.h"
#include "GSTN_HitEffectContext.h"
#include "Rules/GSTN_CombatRules.h"

namespace
{
	FGameplayEffectAttributeCaptureDefinition MakeCapture(const FGameplayAttribute& Attribute, EGameplayEffectAttributeCaptureSource Source)
	{
		return FGameplayEffectAttributeCaptureDefinition(Attribute, Source, false);
	}

	/** The captured value, or Fallback when the owner of the attribute has no such set */
	float Capture(const FGameplayEffectCustomExecutionParameters& Params, const FGameplayEffectAttributeCaptureDefinition& Definition, const FAggregatorEvaluateParameters& Evaluate, float Fallback)
	{
		float Value = Fallback;
		return Params.AttemptCalculateCapturedAttributeMagnitude(Definition, Evaluate, Value) ? Value : Fallback;
	}
}

UGSTN_DamageExecution::UGSTN_DamageExecution()
{
	RelevantAttributesToCapture.Add(MakeCapture(UGSTN_HealthAttributeSet::GetOutgoingDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Source));
	RelevantAttributesToCapture.Add(MakeCapture(UGSTN_HealthAttributeSet::GetIncomingDamageMultiplierAttribute(), EGameplayEffectAttributeCaptureSource::Target));
	RelevantAttributesToCapture.Add(MakeCapture(UGSTN_HealthAttributeSet::GetDamagePercentAttribute(), EGameplayEffectAttributeCaptureSource::Target));
	RelevantAttributesToCapture.Add(MakeCapture(UGSTN_HealthAttributeSet::GetMaxHealthAttribute(), EGameplayEffectAttributeCaptureSource::Target));
}

void UGSTN_DamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters Evaluate;
	Evaluate.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluate.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	const float OutgoingMultiplier = Capture(ExecutionParams, RelevantAttributesToCapture[0], Evaluate, 1.f);
	const float IncomingMultiplier = Capture(ExecutionParams, RelevantAttributesToCapture[1], Evaluate, 1.f);
	const float Damage = FMath::Max(Spec.GetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_Damage, false, 0.f), 0.f) * OutgoingMultiplier * IncomingMultiplier;
	if (Damage <= 0.f)
	{
		return;
	}

	// A plain context carries no rules: the percent mode is the identity of an untuned match
	EGSTN_DamageMode Mode = EGSTN_DamageMode::Percent;
	if (const FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandle(Spec.GetContext()))
	{
		if (const UGSTN_CombatRules* Rules = Context->Rules.Get())
		{
			Mode = Rules->DamageMode;
		}
	}

	switch (Mode)
	{
	case EGSTN_DamageMode::Health:
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UGSTN_HealthAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -Damage));
		break;

	case EGSTN_DamageMode::PercentCapped:
	{
		// Only the room left under the cap is added, so the percent never passes MaxHealth and the KO check sees it exactly at the cap
		const float Current = Capture(ExecutionParams, RelevantAttributesToCapture[2], Evaluate, 0.f);
		const float Cap = Capture(ExecutionParams, RelevantAttributesToCapture[3], Evaluate, 0.f);
		const float Applied = FMath::Clamp(Cap - Current, 0.f, Damage);
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UGSTN_HealthAttributeSet::GetDamagePercentAttribute(), EGameplayModOp::Additive, Applied));
		break;
	}

	case EGSTN_DamageMode::Percent:
	default:
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UGSTN_HealthAttributeSet::GetDamagePercentAttribute(), EGameplayModOp::Additive, Damage));
		break;
	}
}
