// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GSTN_AbilityActivationInput.generated.h"

class UNpGameplayAbility;

/**
 * An input action that activates an ability, as data instead of a path in C++.
 *
 * The fork reads UNpGameplayAbility::ActivationInputs (hard references) from the ability's class default object when an input
 * arrives. This struct is the soft, configurable form of one entry: an ability class keeps one as a Config property, so its
 * ini section (or a Blueprint child's defaults) names the action, and Resolve loads it and fills ActivationInputs when the
 * ability is granted. Nothing in C++ names a content path.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_SoftActivationInput
{
	GENERATED_BODY()

	/** The action that activates the ability. Empty: the ability has no input activation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Activation")
	TSoftObjectPtr<UInputAction> Action;

	/** The event of the action that activates it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Activation")
	ETriggerEvent Trigger = ETriggerEvent::Started;

	/**
	 * Loads Action and adds it to Ability->ActivationInputs once (a second call finds it there and does nothing). Call it on the
	 * class default object, which is what the ability system reads, when the ability is granted.
	 * An unset Action is an error only when bRequired (the ability cannot be activated by input without it): it logs and
	 * returns false, so a missing setup is not silent. An Action that cannot be loaded always logs an error.
	 */
	bool Resolve(UNpGameplayAbility* Ability, bool bRequired) const;
};
