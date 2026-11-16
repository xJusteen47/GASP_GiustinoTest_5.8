// Minion runtime (bridge seam, M8 G10).

#pragma once

#include "CoreMinimal.h"
#include "Store/GSTN_MinionStore.h"

/** One flagged minion at the end of a batch step: the pose a bridge moves its collider proxy to. Feet position, like the store's */
struct FGSTN_MinionPublishedPose
{
	FGSTN_MinionHandle Handle;
	FVector3f Position = FVector3f::ZeroVector;
	float YawDegrees = 0.f;
	FVector3f Velocity = FVector3f::ZeroVector;
};

/**
 * Bridge seam (combat-m8-minions.md 4d.5, "Seams in M8"): told, once per batch step, where the platform-flagged minions
 * (UGSTN_MinionCombatSubsystem::SetPlatformActor) ended the step. The platform bridge moves its collider proxy from here, so the pose
 * is read at one defined point per step (after movement, before the net publish) and never depends on the order Mover instances ticked in.
 *
 * Rules for an observer:
 * - Plain C++ object, not a UObject: the subsystem keeps a raw pointer. The observer MUST call RemovePublishObserver before it is destroyed
 *   (the subsystem's lifetime is the world's, an observer is usually a bridge subsystem of the same world, so its Deinitialize is the place).
 * - Called from inside the batch, on the game thread, with only the flagged and activated slots, in ascending slot order, and only on steps
 *   where at least one such slot exists. Do not spawn, despawn, or flag/unflag minions from the callback (the net publish of the same step
 *   follows); reading the store through the subsystem is fine.
 * - The Poses view is only valid during the call (a reused member array): copy what you keep.
 * - Deterministic input only: the callback must not feed anything back into the batch. A bridge that moves a proxy actor does so after the fact.
 *
 * Inert unbound: with no observer, or no flagged slot, the subsystem does no work for this seam.
 */
class IGSTN_MinionPublishObserver
{
public:
	virtual ~IGSTN_MinionPublishObserver() = default;

	/** Frame is the batch frame whose end the poses are */
	virtual void OnMinionPosesPublished(int32 Frame, TConstArrayView<FGSTN_MinionPublishedPose> Poses) = 0;
};
