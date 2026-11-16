// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * What the broken mask means for the rest of the combat, recomputed every frame by UGSTN_PartMath::ProjectDerived
 * (combat-parts.md §3.3, §5.3, §7). Derived effects, not events: nothing here is stored, so a rollback has nothing to undo.
 * The tick extension of the NP module turns it into projected tags, a hit-group mask for the entity's own hitboxes and
 * hurtbox overrides for the snapshot.
 *
 * Reuse one projection per entity: Reset keeps the containers' capacity, so the steady state allocates nothing.
 */
struct GSTNPARTSCORE_API FGSTN_PartsProjection
{
	/** Tags the Tags break effects of the broken parts grant while they are broken (Combat.Part.Effect.* leaves, designer lock tags) */
	FGameplayTagContainer Granted;

	/** Tags of the definition's base tags the broken parts suppress while broken */
	FGameplayTagContainer Removed;

	/** Hurtbox names the snapshot hides: those of broken parts whose policy is Disable, and those of DisableHurtbox effects */
	TArray<FName> DisabledHurtboxes;

	/** Union of the DisableHitGroups of the broken parts: bit n stops hit group n of the entity's own hitboxes */
	uint8 HitGroupMask = 0;

	/** At least one part is broken (Combat.Part.Broken) */
	bool bAnyBroken = false;

	/** Every breakable part is broken, and there is at least one (Combat.Part.AllBroken) */
	bool bAllBroken = false;

	/** The grab gate of the part set is unmet (Combat.Part.GrabLocked) */
	bool bGrabLocked = false;

	/** Empties the projection, keeping the allocated capacity */
	void Reset();

	/** Adds the native Combat.Part.Broken, GrabLocked and AllBroken tags the three flags stand for */
	void AppendNativeTags(FGameplayTagContainer& OutTags) const;
};
