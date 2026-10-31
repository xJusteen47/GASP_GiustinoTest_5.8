// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestAbilities.h"
#include "GASNPPTest/GSTN_TestEffects.h"

UGSTN_TestAbilityBase::UGSTN_TestAbilityBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGSTN_TestAbilityBase::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// The ability system reads the activation inputs of the class default object, which is what the spec points at
	ActivationInput.Resolve(Cast<UNpGameplayAbility>(Spec.Ability), true);
}

void UGSTN_TestAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (EffectToApply)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, EffectToApply.GetDefaultObject(), GetAbilityLevel(Handle, ActorInfo));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UGSTN_GA_TestDash::UGSTN_GA_TestDash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CostGameplayEffectClass = UGSTN_GE_DashCost::StaticClass();
	CooldownGameplayEffectClass = UGSTN_GE_DashCooldown::StaticClass();
	EffectToApply = UGSTN_GE_Dash::StaticClass();
}

UGSTN_GA_TestBuff::UGSTN_GA_TestBuff(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CooldownGameplayEffectClass = UGSTN_GE_BuffCooldown::StaticClass();
	EffectToApply = UGSTN_GE_Buff::StaticClass();
}
