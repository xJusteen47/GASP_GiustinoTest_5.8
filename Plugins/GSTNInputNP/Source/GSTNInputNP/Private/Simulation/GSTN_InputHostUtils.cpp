// What every host of the input simulation does the same way (GAS-NPP fork).

#include "Simulation/GSTN_InputHostUtils.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GSTN_InputLog.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_InputSimTypes.h"

bool FGSTN_InputHostUtils::CanExecuteBinding(const UAbilitySystemComponent* AbilitySystem, const FGSTN_CommandTable& Table, int32 BindingIndex)
{
	if (!AbilitySystem || !Table.Bindings.IsValidIndex(BindingIndex))
	{
		return false;
	}

	// The same const query the ability system makes when it executes the request
	const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(Table.Bindings[BindingIndex].Ability);
	return Spec && Spec->Ability && Spec->Ability->CanActivateAbility(Spec->Handle, AbilitySystem->AbilityActorInfo.Get());
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
	if (!Spec || !AbilitySystem->TryActivateAbility(Spec->Handle))
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
		Event.Instigator = Owner;
		Subsystem->Broadcast(Event);
	});
}
