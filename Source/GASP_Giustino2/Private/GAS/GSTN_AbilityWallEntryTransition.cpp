// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_AbilityWallEntryTransition.h"
#include "GAS/GSTN_AbilityTags.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "MoverComponent.h"

UGSTN_AbilityWallEntryTransition::UGSTN_AbilityWallEntryTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(GSTNAbilityTags::Ability_Movement_WallSlide);
}

UNpAbilitySystemComponent* UGSTN_AbilityWallEntryTransition::GetAbilitySystem() const
{
	const UMoverComponent* Mover = K2_GetMoverComponent();
	const AActor* Owner = Mover ? Mover->GetOwner() : nullptr;
	return Owner ? Owner->FindComponentByClass<UNpAbilitySystemComponent>() : nullptr;
}

FGameplayAbilitySpecHandle UGSTN_AbilityWallEntryTransition::FindActivatableWallAbility(UNpAbilitySystemComponent* AbilitySystem) const
{
	const FGameplayAbilityActorInfo* ActorInfo = AbilitySystem->AbilityActorInfo.Get();
	if (!ActorInfo)
	{
		return FGameplayAbilitySpecHandle();
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (!Spec.Ability || Spec.PendingRemove || !Spec.Ability->GetAssetTags().HasAny(AbilityTags))
		{
			continue;
		}
		// Already on a wall (e.g. wall to wall): the ability is still active and must not block the next grab
		if (Spec.IsActive() || Spec.Ability->CanActivateAbility(Spec.Handle, ActorInfo))
		{
			return Spec.Handle;
		}
	}
	return FGameplayAbilitySpecHandle();
}

FTransitionEvalResult UGSTN_AbilityWallEntryTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	// Cheap ability check first, the base transition sweeps for walls
	if (UNpAbilitySystemComponent* AbilitySystem = GetAbilitySystem())
	{
		if (!FindActivatableWallAbility(AbilitySystem).IsValid())
		{
			return FTransitionEvalResult::NoTransition;
		}
	}
	else if (!bAllowWithoutAbilitySystem)
	{
		return FTransitionEvalResult::NoTransition;
	}

	return Super::Evaluate_Implementation(Params);
}

void UGSTN_AbilityWallEntryTransition::Trigger_Implementation(const FSimulationTickParams& Params)
{
	Super::Trigger_Implementation(Params);

	UNpAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (!AbilitySystem)
	{
		return;
	}

	const FGameplayAbilitySpecHandle Handle = FindActivatableWallAbility(AbilitySystem);
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
	if (Spec && !Spec->IsActive())
	{
		// Simulated proxies are refused by the ability system: their state comes from the server
		AbilitySystem->TryActivateAbility(Handle);
	}
}
