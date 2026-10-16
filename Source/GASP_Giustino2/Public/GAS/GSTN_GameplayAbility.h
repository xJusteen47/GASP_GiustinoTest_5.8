// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/NpGameplayAbility.h"
#include "GSTN_GameplayAbility.generated.h"

class UMoverComponent;
class UCharacterMoverComponent;

/**
 * Base of the project's gameplay abilities.
 *
 * The GAS-NPP fork only accepts UNpGameplayAbility children: activation, effects and tags are simulated by the Network
 * Prediction plugin (predicted on the owning client, rolled back on corrections). The ability system is sorted before
 * Mover in the fixed tick, so tags and effects granted at a frame are seen by the movement simulation of the same frame.
 *
 * Abilities drive movement through Mover's queued moves (layered moves, instant movement effects), usually from a
 * gameplay effect with UApplyMoveGameplayEffectComponent, never by setting the character state directly.
 */
UCLASS(Abstract)
class GASP_GIUSTINO2_API UGSTN_GameplayAbility : public UNpGameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GameplayAbility(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Ability|Movement")
	UMoverComponent* GetMoverComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Movement")
	UCharacterMoverComponent* GetCharacterMoverFromActorInfo() const;

protected:
	static UMoverComponent* GetMoverComponent(const FGameplayAbilityActorInfo* ActorInfo);

	/** Adds an input action activation trigger. Constructor only (uses ConstructorHelpers) */
	void AddActivationInput(const TCHAR* InputActionPath, ETriggerEvent TriggerEvent = ETriggerEvent::Started);
};
