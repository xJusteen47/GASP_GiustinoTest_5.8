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
 * Creates the drill's plunge (D8, spin-drill.md 7.4, 14 SD.6): the velocity becomes PlungeSpeed along the entity's gravity (the tracker's down,
 * any direction), the speed across it dropped. With the dive gravity effect (no gravity while diving) the plunge keeps its speed; the
 * lateral steering is the air control the character always has (the 15 % of spec 13b is a playtest number for it).
 */
UCLASS(DisplayName = "GSTN Drill Plunge Creator")
class GSTNSPINDRILLNP_API UGSTN_DrillPlungeCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: the plunge of the dive (UGSTN_DrillPlungeCreator). Applied by the dive ability when its timeline starts */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_DrillPlunge : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_DrillPlunge();
};

/** Creates the bounce off a hittable target that is not drillable (spin-drill.md 7.6): BounceSpeed against gravity, the speed across it kept */
UCLASS(DisplayName = "GSTN Drill Bounce Creator")
class GSTNSPINDRILLNP_API UGSTN_DrillBounceCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: the bounce of the dive (UGSTN_DrillBounceCreator). Applied by the dive ability on the bounce frame */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_DrillBounce : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_DrillBounce();
};

/**
 * Creates the launch out of a bore (D9 part 2, spin-drill.md 7.4): LaunchSpeed along the tuning's LaunchDirection, in the falling mode, so the
 * parked mode of the latch link does not keep it. The surface normal is the driller's up: the dive goes along gravity, so the tip meets the
 * target's face that looks against it (a side-on latch reads as the top; the anchor's direction would need the target's pose here)
 */
UCLASS(DisplayName = "GSTN Drill Launch Creator")
class GSTNSPINDRILLNP_API UGSTN_DrillLaunchCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: the launch of a bore (UGSTN_DrillLaunchCreator). Applied by the bore ability on the frame the fragment enters Launching */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_DrillLaunch : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_DrillLaunch();
};

/** Creates the detach pop (crouch released before the launch is earned): a hop of DetachPopSpeed against gravity, in the falling mode */
UCLASS(DisplayName = "GSTN Drill Pop Creator")
class GSTNSPINDRILLNP_API UGSTN_DrillPopCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
		const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const override;
};

/** Instant: the detach pop (UGSTN_DrillPopCreator). Applied by the bore ability on the frame the fragment carries the Popped flag */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_DrillPop : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_DrillPop();
};

/**
 * The dive gravity (D8): an infinite effect that sets the jump and fall gravity scales to zero while the fragment is Diving, so the plunge keeps
 * its speed whatever the jump phase would apply. Grants Combat.State.DiveGravity and is synced from the fragment every frame like the low
 * gravity (level-triggered, one tag test). A pawn without the combat attribute set is left untouched.
 */
UCLASS()
class GSTNSPINDRILLNP_API UGSTN_GE_DrillDiveGravity : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_DrillDiveGravity();

	/** Applies the effect when wanted and absent, removes it when present and not wanted. Returns whether it changed anything */
	static bool Sync(UAbilitySystemComponent& AbilitySystem, bool bWanted);
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
