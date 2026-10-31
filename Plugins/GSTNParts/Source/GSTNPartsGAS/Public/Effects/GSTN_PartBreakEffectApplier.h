// Destructible parts, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Attributes/GSTN_KitStats.h"
#include "Data/GSTN_PartDefinition.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GSTN_GE_PartStats.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGSTN_PartBreakEffect_Launch;

/**
 * The recoil of a break as plain numbers, in the shape of the M4 reaction move (FGSTN_ReactionMove): a world velocity, the decay of
 * its horizontal speed, a gravity scale and the lifetime of the move. This module cannot build the layered move (that needs the
 * Mover), so the NP half turns the request into the same knockback effect the hit reaction uses; the numbers are what both
 * sides agree on and what the tests pin.
 */
struct GSTNPARTSGAS_API FGSTN_PartLaunchRequest
{
	/** World velocity, cm/s: the hit's horizontal direction times Speed x (1 - UpFraction), plus the entity's up times Speed x UpFraction */
	FVector Velocity = FVector::ZeroVector;

	/** Fraction of the horizontal speed lost per frame after the first; 0 = none (a break recoil has no drag of its own) */
	float DecayPerFrame = 0.f;

	/** 1 = the gravity of the movement mode */
	float GravityScale = 1.f;

	/** Lifetime of the move in frames */
	int32 Frames = 0;

	/** False for the empty request: nothing is launched */
	bool IsSet() const { return !Velocity.IsNearlyZero(); }
};

/**
 * The set-by-caller keys the knockback effect of the NP half reads (Combat.SetByCaller.MoveVelocityX...). They are declared by
 * GSTNCombatNP, which this module cannot include, so the caller that owns the knockback effect passes them in. Invalid tags are
 * skipped, so a partial set is harmless.
 */
struct GSTNPARTSGAS_API FGSTN_PartLaunchKeys
{
	FGameplayTag VelocityX;
	FGameplayTag VelocityY;
	FGameplayTag VelocityZ;
	FGameplayTag DecayPerFrame;
	FGameplayTag GravityScale;
	FGameplayTag Seconds;
};

/** The one-shot effects the parts newly broken by one hit ask for, collected in part index order */
struct GSTNPARTSGAS_API FGSTN_PartBreakRequests
{
	/** Parts with a Stats effect among the newly broken ones, ascending */
	TArray<int32, TInlineAllocator<4>> StatsParts;

	/** The strongest Launch effect among the newly broken parts (ties: the lowest part index); not set if none launches */
	FGSTN_PartLaunchRequest Launch;

	/** The longest Stagger effect among the newly broken parts: hitstun of at least this many frames; 0 = none */
	int32 MinimumHitstunFrames = 0;
};

/**
 * Applies the one-shot break effects of combat-parts.md §3.3 (Stats, Launch, Stagger) for the parts a hit newly broke. Static and
 * stateless: the outcome applier of the NP half calls it at the victim's tick, with the newly broken mask the part math returned,
 * so a resimulated hit asks for the same effects. MainDamage is NOT handled here: it is already folded into the hit's main delta by
 * the part math, and applying it a second time would double count. The derived effects (Tags, DisableHurtbox, DisableHitGroups)
 * are projections, see FGSTN_PartTagProjection and the part math.
 *
 * Nothing here reads a component other than the ability system it is handed, and nothing branches on the network role.
 */
struct GSTNPARTSGAS_API FGSTN_PartBreakEffectApplier
{
	/** Visits the set bits of NewlyBrokenMask in ascending order and collects the one-shot effects of those parts. Allocation free for up to four stats parts */
	static void Collect(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 NewlyBrokenMask, const FVector& Up, const FVector& HitDirection,
		int32 LaunchFrames, FGSTN_PartBreakRequests& Out);

	/**
	 * The launch of one Launch effect. The horizontal direction is the hit's direction projected onto the plane normal to Up
	 * (never world Z); a hit along Up has none and the request is then only the vertical part. Up need not be normalised.
	 */
	static FGSTN_PartLaunchRequest MakeLaunchRequest(const FGSTN_PartBreakEffect_Launch& Effect, const FVector& Up, const FVector& HitDirection, int32 Frames);

	/** The longest Stagger effect among the parts in Mask, 0 when none has one. A negative frame count counts as 0 */
	static int32 GetMinimumStaggerFrames(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 Mask);

	/** The hitstun a hit and a break agree on: at least the break's frames, never less than the hit's own */
	static int32 ApplyMinimumHitstun(int32 HitstunFrames, int32 MinimumFrames) { return FMath::Max(HitstunFrames, MinimumFrames); }

	/**
	 * The magnitudes of every Stats effect of a part, merged by stat key (equal keys add up, in the order of the data). Empty when
	 * the part has none. A key that is not valid is dropped
	 */
	static void CollectStatValues(const FGSTN_PartDefinition& Part, TArray<FGSTN_StatValue>& Out);

	/**
	 * Applies the Stats effect of every part in Mask that has one, as an infinite effect of EffectClass per part. Parts already
	 * carrying their effect keep it (no stacking on a resimulation). Returns the number of effects applied by this call
	 */
	static int32 ApplyStats(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, TConstArrayView<FGSTN_PartDefinition> Parts, uint16 Mask);

	/** Removes the Stats effect of every part in Mask (a restore). Returns the number removed */
	static int32 RemoveStats(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, uint16 Mask);

	/**
	 * Builds the spec of the knockback effect for a launch request, the way the hit reaction does: the velocity components, the decay,
	 * the gravity scale and the lifetime in seconds as set-by-caller magnitudes. Invalid when the class is null or the request empty.
	 * StepMs is the simulation step in milliseconds
	 */
	static FGameplayEffectSpecHandle MakeLaunchSpec(UAbilitySystemComponent& ASC, TSubclassOf<UGameplayEffect> EffectClass,
		const FGSTN_PartLaunchRequest& Request, float StepMs, const FGSTN_PartLaunchKeys& Keys);
};
