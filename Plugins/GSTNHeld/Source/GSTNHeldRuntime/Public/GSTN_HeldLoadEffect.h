// Held objects, the carry load effect (M10 G6).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayModMagnitudeCalculation.h"

#include "GSTN_HeldLoadEffect.generated.h"

/**
 * The walk speed multiplier of the carry load: decodes the Held link's payload from the set-by-caller Combat.SetByCaller.Held.Load (payload / 255)
 * and returns FGSTN_HeldLoad::SpeedScale for the payload's size class (UGSTN_HeldRules::SizeClasses) at its load ratio. A pure read of the spec
 * and the project settings: the same link gives the same magnitude on every machine and in a re-simulation
 */
UCLASS()
class GSTNHELDRUNTIME_API UGSTN_HoldLoadSpeedScale : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};

/** The jump speed multiplier of the carry load (FGSTN_HeldLoad::JumpScale), read as UGSTN_HoldLoadSpeedScale */
UCLASS()
class GSTNHELDRUNTIME_API UGSTN_HoldLoadJumpScale : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};

/**
 * The carry load (plan G6, spec 7 GE_HoldLoad): the Held link type's holder role effect (FGSTN_LinkRoleDef::LinkEffect), so GSTNLink's link tick
 * applies it on the frame the hold becomes Active with the link's payload as the set-by-caller magnitude and removes it on every end of the hold,
 * whatever the reason (drop, throw, hit, KO, watchdog, a corrected link). Infinite; multiplies WalkSpeed (read into Mover's max speed by the
 * stats modifier) and the jump speeds (read by the jump abilities). No new executor and no state of its own.
 *
 * Make a Blueprint child for a cue or extra tags and name it in place of this class only by changing the type's definition (FGSTN_HeldLinkType).
 */
UCLASS(Blueprintable)
class GSTNHELDRUNTIME_API UGSTN_GE_HoldLoad : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_HoldLoad();

	/** The speed and jump multipliers a payload gives (the MMCs' math, for tests and the debug report) */
	static float SpeedScaleOf(uint8 Payload);
	static float JumpScaleOf(uint8 Payload);
};
