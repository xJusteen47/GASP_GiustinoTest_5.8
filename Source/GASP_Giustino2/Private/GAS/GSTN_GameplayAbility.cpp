// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_GameplayAbility.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"

UGSTN_GameplayAbility::UGSTN_GameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

UMoverComponent* UGSTN_GameplayAbility::GetMoverComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar ? Avatar->FindComponentByClass<UMoverComponent>() : nullptr;
}

const UGSTN_MoverSimStateTracker* UGSTN_GameplayAbility::GetMoverSimState(const FGameplayAbilityActorInfo* ActorInfo)
{
	return UGSTN_MoverSimStateTracker::FindOrAdd(GetMoverComponent(ActorInfo));
}

void UGSTN_GameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// Start tracking before the first simulation frame that uses it
	GetMoverSimState(ActorInfo);

	// The ability system reads the activation inputs of the class default object, which is what the spec points at
	ActivationInput.Resolve(Cast<UNpGameplayAbility>(Spec.Ability), bRequiresActivationInput);
}

UMoverComponent* UGSTN_GameplayAbility::GetMoverComponentFromActorInfo() const
{
	return GetMoverComponent(GetCurrentActorInfo());
}

UCharacterMoverComponent* UGSTN_GameplayAbility::GetCharacterMoverFromActorInfo() const
{
	return Cast<UCharacterMoverComponent>(GetMoverComponentFromActorInfo());
}

