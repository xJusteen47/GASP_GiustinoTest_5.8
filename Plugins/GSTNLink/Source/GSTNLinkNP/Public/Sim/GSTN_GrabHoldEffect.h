// Links, Network Prediction half: the grabber's walk and jump while holding (grab-link).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GSTN_GrabHoldEffect.generated.h"

/** The walk speed multiplier while holding: UGSTN_LinkSettings::GetEffectiveHoldWalkScale (gstn.Grab.WalkScale over HoldWalkScale) */
UCLASS()
class GSTNLINKNP_API UGSTN_GrabHoldWalkScale : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};

/** The jump speed multiplier while holding (owner 2026-11-23: "salto più basso"): GetEffectiveHoldJumpScale */
UCLASS()
class GSTNLINKNP_API UGSTN_GrabHoldJumpScale : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};

/**
 * grab-link (owner 2026-11-23): while holding a grabbed enemy the grabber WALKS and JUMPS, lower. The grab type's GRABBER role effect
 * (FGSTN_LinkRoleDef::LinkEffect on the definition), so the link tick applies it on the frame the grab becomes Active and removes it on every end
 * (throw, put-down, escape, hit, KO, watchdog), on every machine and in every re-simulation: the same path as GSTNHeld's UGSTN_GE_HoldLoad, whose
 * shape this copies. Infinite; MultiplyCompound on WalkSpeed (read into Mover's max speed) and the jump speeds (read by the jump abilities).
 *
 * The scales are project settings with a cvar each in the GSTN Debug window, read when the effect is applied (the start of each hold): every machine
 * must have the same values (PIE: one process). Nothing else changes: the grabber keeps its locomotion, its crouch (no link animation change in crouch
 * for now, owner) and its jump; the parked partner follows the grabber's root through the parked mode.
 */
UCLASS(Blueprintable)
class GSTNLINKNP_API UGSTN_GE_GrabHold : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_GrabHold();
};
