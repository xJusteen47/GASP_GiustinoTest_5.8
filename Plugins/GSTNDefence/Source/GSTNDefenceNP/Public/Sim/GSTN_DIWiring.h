// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGSTN_DefenceRules;
struct FGSTN_CombatTickContext;
struct FGSTN_DefenceFragment;
struct FGSTN_HitContextView;

/**
 * The defence's DI rules wired into the combat model (m6 spec 5.5, M6 F8). The core applies DI once, at the end of a launch's hitstop, through
 * its existing hit-state path (FGSTN_CombatVictim::StepReaction); the defence tick extension answers the core's OverrideDI seam for an entity
 * that has defence data, so the launch is bent by UGSTN_DIRulesMath::ApplyDI (the strength curve, the victim tag scale, the elevation curve,
 * MinKnockbackForDI) with the rules' parameters instead of the combat rules' plain FGSTN_DIParams. A hit whose data says bDisableDI keeps
 * its launch: the outcome applier records that, with the SDI permission, in the fragment's HitFlags at every hit (MakeHitFlags), because the
 * hit data itself is gone when the hitstop ends.
 *
 * Simulation code: pure functions of the launch, the frame's stick, the fragment and content (the rules, the victim's tags at the end of the
 * previous frame). No net mode, no resimulation branch.
 */
struct GSTNDEFENCENP_API FGSTN_DIWiring
{
	/**
	 * The GSTNDefenceHitFlags of a resolved hit: DIDisabled from the hit data, SDIBlocked for a grab-like hit (throw), an armored hurtbox the
	 * rules keep still (bSDIAllowedWhenArmored false) or the stagger-in-place tier where the rules say so (bSDIInStaggerInPlace false). The
	 * victim's rules come from its snapshot extension; a victim without defence gets 0
	 */
	static uint8 MakeHitFlags(const FGSTN_HitContextView& View);

	/** UGSTN_DIRulesMath::ApplyDI with Rules.DI, the knockback magnitude of the launch and the fragment's DIDisabled flag */
	static FVector ApplyRulesDI(const UGSTN_DefenceRules& Rules, const FGSTN_DefenceFragment* Fragment, const FVector& Launch, const FVector& StickOnPlane, const FVector& Up,
		const FVector& YawFallback, const FGameplayTagContainer& VictimTags);

	/** The tick extension's OverrideDI: false (the core's DI) for an entity without defence data */
	static bool OverrideDI(const FGSTN_CombatTickContext& Context, const FVector& Launch, const FVector& StickOnPlane, const FVector& Up, const FVector& YawFallback, FVector& OutVelocity);

	/** The victim's owned tags at the end of the previous frame (the ability system ticks after the combat model), empty without one */
	static FGameplayTagContainer ReadVictimTags(const FGSTN_CombatTickContext& Context);
};
