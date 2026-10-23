// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GAS/GSTN_GameplayAbility.h"
#include "GameplayEffect.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GSTN_GA_Jump.generated.h"

/**
 * Creates Mover's jump impulse (FJumpImpulseEffect, the same instant effect UCharacterMoverComponent::Jump queues):
 * overrides the vertical velocity and switches to the air mode of the common legacy settings.
 *
 * The upward speed is, in order: UpwardsSpeed when set, the JumpVelocity stat when the ability system has the combat attribute set
 * (so the project jump and the combat kit's UGSTN_GA_PlatformJump reach the same height), JumpUpwardsSpeed of the Mover settings.
 */
UCLASS(DisplayName = "GSTN Jump Impulse Move Creator")
class GASP_GIUSTINO2_API UGSTN_JumpImpulseMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	/** Upward speed (cm/s). 0 = use JumpUpwardsSpeed from the Mover's UCommonLegacyMovementSettings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float UpwardsSpeed = 0.f;

	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: queues the jump impulse on the target's Mover component */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GE_Jump : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_Jump();
};

/**
 * Ground jump as a gameplay ability. Input: IA_Jump (Started). TO BE REPLACED LATER by the combat plugin's UGSTN_GA_PlatformJump
 * (variable height, air jump, fast fall) for characters with a combat kit; kept for the ones without.
 *
 * Activation applies JumpEffect (by default UGSTN_GE_Jump, which queues Mover's jump impulse) and ends. Costs,
 * cooldowns and blocking/cancel tags are the standard UGameplayAbility settings, so they can be set on a Blueprint child.
 *
 * Once granted it owns the ground jump: UCharacterMoverComponent's built-in jump handling (bHandleJump) is turned off on
 * the avatar so the same press does not jump twice. Jumps off walls stay with the wall movement modes (WallJump
 * settings), which read the jump press from Mover's own input.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_Jump : public UGSTN_GameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GA_Jump(const FObjectInitializer& ObjectInitializer);

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

protected:
	/** Effect applied to the owner on activation. It must move the character (e.g. UApplyMoveGameplayEffectComponent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	TSubclassOf<UGameplayEffect> JumpEffect;

	/** Only from the ground (UCharacterMoverComponent::CanActorJump) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	bool bRequireGround = true;

	/** Turn off the Mover component's built-in jump on the avatar, so only this ability jumps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	bool bTakeOverMoverJump = true;

private:
	void TakeOverMoverJump(const FGameplayAbilityActorInfo* ActorInfo) const;
};
