// Backpacks, Network Prediction half (M11 L7d: the dome pushes back whoever hits it from outside).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_AttackerPush.h"

class UAbilitySystemComponent;
struct FGSTN_BackpackSnapshotExt;

/**
 * The push back of a melee attacker whose hit a backpack dome blocked (owner 2026-11-13). The blocking dome is found again from the snapshot the
 * hit was found against, the way the BackpackDome step found it: the shell (an extra hurtbox of the backpack) is its carrier's dome; a hit on a
 * body inside domes is blocked by the first of them, in owner id order, that does not hold the attacker's root. The push is that dome's data
 * (FGSTN_BackpackDomeTrack::PushbackSpeed, PushbackFrames, PushbackDirection, carried by the snapshot blob): on the attacker's movement plane,
 * out of the dome or back along the attack.
 *
 * A fighter attacker gets it from the backpack's combat tick extension on the frame of the hit (a push back move through its ability system,
 * like the shield's pushback); a minion attacker through FGSTN_AttackerPushRegistry. A projectile is never pushed (it is just spent).
 */
class GSTNBACKPACKNP_API FGSTN_BackpackDomePush : public IGSTN_AttackerPushExtension
{
public:
	/** Fixed tick of the model, seconds: the push move's lifetime is frames times this */
	static constexpr float FrameSeconds = 1.f / 62.5f;

	virtual uint8 GetTypeId() const override;
	virtual bool GetPush(const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames) const override;

	/** The dome that blocked the hit of the query, as its owner's blob and id; null when no dome did */
	static const FGSTN_BackpackSnapshotExt* FindBlockingDome(const FGSTN_AttackerPushQuery& Query, int32& OutOwnerId);

	/** The push of one dome on an attacker (zero speed or frames: none) */
	static bool ComputePush(const FGSTN_BackpackSnapshotExt& Dome, const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames);

	/** Queues the push on a fighter's Mover through its ability system (UGSTN_GE_PushBack, the move the shield's pushback uses) */
	static void ApplyToFighter(UAbilitySystemComponent* AbilitySystem, const FVector& Velocity, int32 Frames);
};
