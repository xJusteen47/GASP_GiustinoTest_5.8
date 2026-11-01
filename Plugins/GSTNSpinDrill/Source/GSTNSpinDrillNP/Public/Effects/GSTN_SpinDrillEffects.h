// Spin and drill, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"
#include "GameplayEffect.h"
#include "GSTN_SpinDrillEffects.generated.h"

class UAbilitySystemComponent;
struct FGSTN_DrillSimTuning;

/**
 * Creates the hop of the air spin (spin-drill.md 7.3) as an instant velocity effect: UGSTN_DrillMath::ComputeHopVelocity on the velocity and
 * the up direction of the Mover sim state tracker (the end of the previous frame, the same on every machine and in every re-simulation),
 * with the hop speed of the entity's attached tuning (FGSTN_SpinDrillAttach, the defaults without one). The movement mode is kept, so a
 * hop never invents an air mode: the air spin only runs in the air anyway. Not a launch move, so the jump state does not read it as a
 * trampoline (which would refresh the air spin it just spent).
 */
UCLASS(DisplayName = "GSTN Air Spin Hop Creator")
class GSTNSPINDRILLNP_API UGSTN_AirSpinHopCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: the hop of the air spin (UGSTN_AirSpinHopCreator). Applied by the air spin ability when its timeline starts */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_AirSpinHop : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_AirSpinHop();
};

/**
 * The low gravity of the air spin (spin-drill.md 7.3, 14 SD.3) without a core seam: an infinite effect that multiplies the combat stats the
 * jump logic turns into gravity every frame (JumpRiseGravityScale, JumpCutGravityScale, FallGravityScale by the gravity SetByCaller,
 * MaxFallSpeed by the fall speed SetByCaller), so the stats modifier applies it with the rest of the jump gravity. It grants
 * Combat.State.SpinLowGravity, which the drill tick extension compares with the fragment's low gravity window every frame: the effect is
 * applied when the window holds and the tag is missing, removed in the opposite case. Level-triggered on purpose, so a corrected or rolled
 * back spin can never leave the effect behind. A pawn without the combat attribute set is left untouched.
 */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_AirSpinLowGravity : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_AirSpinLowGravity();

	/**
	 * Makes the ability system's low gravity follow bWanted: applies the effect with the tuning's scales when wanted and absent, removes it
	 * when present and not wanted, does nothing otherwise (one tag test). Returns whether it changed anything
	 */
	static bool Sync(UAbilitySystemComponent& AbilitySystem, bool bWanted, const FGSTN_DrillSimTuning& Tuning);
};
