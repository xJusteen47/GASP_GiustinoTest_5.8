// Defence, ability-system half (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_ShieldRestore.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GSTN_DefenceAttributeSet.h"
#include "GameFramework/Actor.h"

UGSTN_GE_ShieldRestore::UGSTN_GE_ShieldRestore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Amount;
	Amount.DataName = GetAmountKey();

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGSTN_DefenceAttributeSet::GetShieldHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
	Modifiers.Add(Modifier);
}

FName UGSTN_GE_ShieldRestore::GetAmountKey()
{
	static const FName Key(TEXT("GSTN.Defence.ShieldRestore.Amount"));
	return Key;
}

bool UGSTN_GE_ShieldRestore::Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_ShieldRestore> EffectClass, float Amount, AActor* Instigator)
{
	const TSubclassOf<UGameplayEffect> Class = EffectClass ? TSubclassOf<UGameplayEffect>(EffectClass) : TSubclassOf<UGameplayEffect>(StaticClass());
	FGameplayEffectContextHandle Context = ASC.MakeEffectContext();
	Context.AddInstigator(Instigator ? Instigator : ASC.GetOwner(), nullptr);
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(Class, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(GetAmountKey(), Amount);
	return ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data).WasSuccessfullyApplied();
}

float UGSTN_GE_ShieldRestore::RequestedDelta(const UAbilitySystemComponent& ASC, float LastMirroredValue)
{
	const FGameplayAttribute Mirror = UGSTN_DefenceAttributeSet::GetShieldHealthAttribute();
	if (!ASC.HasAttributeSetForAttribute(Mirror))
	{
		return 0.f;
	}
	return ASC.GetNumericAttributeBase(Mirror) - LastMirroredValue;
}
