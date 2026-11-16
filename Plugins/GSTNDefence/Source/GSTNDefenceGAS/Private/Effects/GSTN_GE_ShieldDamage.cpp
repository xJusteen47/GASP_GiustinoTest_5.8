// Defence, ability-system half (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_ShieldDamage.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "GSTN_DefenceTags.h"

float UGSTN_ShieldDamageMagnitude::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return Spec.GetSetByCallerMagnitude(GSTNDefenceTags::SetByCaller_ShieldDamage, false, 0.f);
}

UGSTN_GE_ShieldDamage::UGSTN_GE_ShieldDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Additive with a coefficient of -1: the key is a loss, and a set-by-caller magnitude alone cannot change sign
	FCustomCalculationBasedFloat Loss;
	Loss.CalculationClassMagnitude = UGSTN_ShieldDamageMagnitude::StaticClass();
	Loss.Coefficient = -1.f;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGSTN_DefenceAttributeSet::GetShieldHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Loss);
	Modifiers.Add(Modifier);
}

bool UGSTN_GE_ShieldDamage::Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_ShieldDamage> EffectClass, float Loss)
{
	const TSubclassOf<UGameplayEffect> Class = EffectClass ? TSubclassOf<UGameplayEffect>(EffectClass) : TSubclassOf<UGameplayEffect>(StaticClass());
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(Class, 1.f, ASC.MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNDefenceTags::SetByCaller_ShieldDamage, Loss);
	return ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data).WasSuccessfullyApplied();
}

bool UGSTN_GE_ShieldDamage::ApplyMirror(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_ShieldDamage> EffectClass, float NewValue)
{
	if (!ASC.HasAttributeSetForAttribute(UGSTN_DefenceAttributeSet::GetShieldHealthAttribute()))
	{
		return false;
	}
	const float Current = ASC.GetNumericAttributeBase(UGSTN_DefenceAttributeSet::GetShieldHealthAttribute());
	return Apply(ASC, EffectClass, Current - NewValue);
}
