// What every host of the input simulation does the same way (GAS-NPP fork).

#include "Simulation/GSTN_InputHostUtils.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_InputLog.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_InputSimTypes.h"

namespace
{
	/** The specs a command is being activated for, innermost last. Scopes are stack objects, so they pop in reverse order */
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<4>>& GetCommandActivations()
	{
		static TArray<FGameplayAbilitySpecHandle, TInlineAllocator<4>> Activations;
		return Activations;
	}
}

FGSTN_CommandActivationScope::FGSTN_CommandActivationScope(const FGameplayAbilitySpecHandle& Handle)
{
	GetCommandActivations().Add(Handle);
}

FGSTN_CommandActivationScope::~FGSTN_CommandActivationScope()
{
	GetCommandActivations().Pop(EAllowShrinking::No);
}

bool FGSTN_InputHostUtils::IsCommandActivation(const FGameplayAbilitySpecHandle& Handle)
{
	return Handle.IsValid() && GetCommandActivations().Contains(Handle);
}

bool FGSTN_InputHostUtils::CanExecuteBinding(const UAbilitySystemComponent* AbilitySystem, const FGSTN_CommandTable& Table, int32 BindingIndex)
{
	if (!AbilitySystem || !Table.Bindings.IsValidIndex(BindingIndex))
	{
		return false;
	}

	// The same const query the ability system makes when it executes the request
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(Table.Bindings[BindingIndex].Ability);
	if (!Spec || !Spec->Ability)
	{
		return false;
	}
	const FGSTN_CommandActivationScope CommandScope(Spec->Handle);
	return Spec->Ability->CanActivateAbility(Spec->Handle, AbilitySystem->AbilityActorInfo.Get());
}

bool FGSTN_InputHostUtils::ExecuteRequest(UAbilitySystemComponent* AbilitySystem, const FGSTN_CommandTable& Table, const FGSTN_InputSimState& State, int32 Frame)
{
	// RequestedBinding is the 1-based index in the command table: the context already chose the binding
	const int32 BindingIndex = static_cast<int32>(State.RequestedBinding) - 1;
	if (State.RequestFrame != Frame || !Table.Bindings.IsValidIndex(BindingIndex))
	{
		return false;
	}

	const TSubclassOf<UGameplayAbility>& Ability = Table.Bindings[BindingIndex].Ability;
	const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromClass(Ability) : nullptr;
	bool bActivated = false;
	if (Spec)
	{
		// Copied: the ability system's spec array can move while the activation runs
		const FGameplayAbilitySpecHandle Handle = Spec->Handle;
		const FGSTN_CommandActivationScope CommandScope(Handle);
		bActivated = AbilitySystem->TryActivateAbility(Handle);
	}
	if (!bActivated)
	{
		UE_LOG(LogGSTNInput, Verbose, TEXT("Frame %d: binding %d (%s) did not activate"), Frame, BindingIndex + 1, *GetNameSafe(Ability.Get()));
	}
	return true;
}

bool FGSTN_InputHostUtils::IsButtonHeld(const FGSTN_InputSimState& State, int32 Button)
{
	return Button >= 0 && Button < 32 && (State.History.GetHeldButtons() & (1u << Button)) != 0;
}

int32 FGSTN_InputHostUtils::PublishOutbox(FGSTN_SimEventOutbox& Outbox, AActor* Owner)
{
	const bool bAuthority = Owner && Owner->HasAuthority();
	UGSTN_SimEventSubsystem* Subsystem = bAuthority ? UGSTN_SimEventSubsystem::Get(Owner) : nullptr;

	return Outbox.Flush(bAuthority && Subsystem != nullptr, [Owner, Subsystem](FGSTN_SimEvent& Event)
	{
		// An event that names its own instigator (a hit's attacker) keeps it; the others are the owner's
		if (!Event.Instigator.IsValid())
		{
			Event.Instigator = Owner;
		}
		Subsystem->Broadcast(Event);
	});
}
