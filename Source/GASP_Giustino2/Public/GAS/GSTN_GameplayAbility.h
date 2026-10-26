// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/NpGameplayAbility.h"
#include "GAS/GSTN_AbilityActivationInput.h"
#include "GSTN_GameplayAbility.generated.h"

class UMoverComponent;
class UCharacterMoverComponent;
class UGSTN_MoverSimStateTracker;

/**
 * Base of the project's gameplay abilities.
 *
 * The GAS-NPP fork only accepts UNpGameplayAbility children: activation, effects and tags are simulated by the Network
 * Prediction plugin (predicted on the owning client, rolled back on corrections). The ability system is sorted before
 * Mover in the fixed tick, so tags and effects granted at a frame are seen by the movement simulation of the same frame.
 *
 * Abilities drive movement through Mover's queued moves (layered moves, instant movement effects), usually from a
 * gameplay effect with UApplyMoveGameplayEffectComponent, never by setting the character state directly.
 *
 * To read the movement state (mode, on ground...) from the simulation use GetMoverSimState, not the Mover component's own
 * queries: those return the game frame state, which is wrong while re-simulating (see UGSTN_MoverSimStateTracker).
 */
UCLASS(Abstract, Config = Game)
class GASP_GIUSTINO2_API UGSTN_GameplayAbility : public UNpGameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GameplayAbility(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Ability|Movement")
	UMoverComponent* GetMoverComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Movement")
	UCharacterMoverComponent* GetCharacterMoverFromActorInfo() const;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	static UMoverComponent* GetMoverComponent(const FGameplayAbilityActorInfo* ActorInfo);

	/** Mover's simulation state as of the last simulated frame. Null if the avatar has no Mover component */
	static const UGSTN_MoverSimStateTracker* GetMoverSimState(const FGameplayAbilityActorInfo* ActorInfo);

	/**
	 * The input action that activates this ability (UNpGameplayAbility::ActivationInputs is filled from it when the ability is
	 * granted). Config, so a class names its action in its ini section (Config/DefaultGame.ini, [/Script/GASP_Giustino2.<Class>]
	 * ActivationInput=(Action=...,Trigger=...)) or a Blueprint child sets it in its defaults; no content path in C++.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Ability|Activation")
	FGSTN_SoftActivationInput ActivationInput;

	/** True for an ability that is activated by ActivationInput: an unset action is then logged as an error when it is granted */
	bool bRequiresActivationInput = false;
};
