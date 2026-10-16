// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_GameplayAbility.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

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

UMoverComponent* UGSTN_GameplayAbility::GetMoverComponentFromActorInfo() const
{
	return GetMoverComponent(GetCurrentActorInfo());
}

UCharacterMoverComponent* UGSTN_GameplayAbility::GetCharacterMoverFromActorInfo() const
{
	return Cast<UCharacterMoverComponent>(GetMoverComponentFromActorInfo());
}

void UGSTN_GameplayAbility::AddActivationInput(const TCHAR* InputActionPath, ETriggerEvent TriggerEvent)
{
	ConstructorHelpers::FObjectFinder<UInputAction> InputActionFinder(InputActionPath);
	if (InputActionFinder.Succeeded())
	{
		FAbilityActivationTrigger Trigger;
		Trigger.InputAction = InputActionFinder.Object;
		Trigger.TriggerEvent = TriggerEvent;
		ActivationInputs.Add(Trigger);
	}
}
