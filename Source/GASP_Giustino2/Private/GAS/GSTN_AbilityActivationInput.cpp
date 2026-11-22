// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_AbilityActivationInput.h"
#include "Abilities/NpGameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNAbilityInput, Log, All);

bool FGSTN_SoftActivationInput::Resolve(UNpGameplayAbility* Ability, bool bRequired) const
{
	if (!Ability)
	{
		return false;
	}

	if (Action.IsNull())
	{
		UE_CLOG(bRequired, LogGSTNAbilityInput, Error, TEXT("%s needs an activation input action and has none: set ActivationInput in its ini section or Blueprint defaults. It cannot be activated by input."),
			*GetNameSafe(Ability->GetClass()));
		return !bRequired;
	}

	UInputAction* Loaded = Action.LoadSynchronous();
	if (!Loaded)
	{
		UE_LOG(LogGSTNAbilityInput, Error, TEXT("%s: activation input action %s could not be loaded. It cannot be activated by input."), *GetNameSafe(Ability->GetClass()), *Action.ToString());
		return false;
	}

	for (const FAbilityActivationTrigger& Existing : Ability->ActivationInputs)
	{
		if (Existing.InputAction == Loaded && Existing.TriggerEvent == Trigger)
		{
			return true;
		}
	}

	FAbilityActivationTrigger NewTrigger;
	NewTrigger.InputAction = Loaded;
	NewTrigger.TriggerEvent = Trigger;
	Ability->ActivationInputs.Add(NewTrigger);
	return true;
}
