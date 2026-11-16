// Minion runtime: movement backends (authority batch stage 4, M8 G4).

#pragma once

#include "CoreMinimal.h"
#include "Movement/GSTN_MinionMovementBackend.h"

struct FHitResult;
struct FGSTN_MinionStats;

/**
 * Backend B of combat-m8-minions.md 6: a kinematic capsule per slot, no component, no physics body. Per slot and frame:
 *   1. velocity: a pending launch (Launched: as is; PushedBack: projected on the ground plane), else the intent's ground velocity
 *      (accelerated at the type's rate) plus the minion-minion separation push; gravity when not grounded; push back decays.
 *   2. move: one capsule sweep over the whole displacement (so 3000 cm/s cannot tunnel a 10 cm wall: the sweep covers it), then
 *      one slide along the hit plane with a second sweep for the remainder. A blocking hit on a low obstacle while walking tries
 *      a step up (up, forward, down: the floor reached must be walkable and within StepHeight). A slot that starts a frame inside
 *      geometry is first pushed out along the penetration's minimum translation (overlap + ComputePenetration).
 *   3. floor: grounded slots sweep down within StepHeight and snap; no floor = Walking falls off the ledge, PushedBack stops at
 *      the ledge (moved back to the start of the frame) and ends. Falling slots land on a walkable hit while descending.
 *   4. kill Z: feet below the world's KillZ are reported in Ctx.FellOut.
 *
 * Typical cost: 2 sweeps per moving minion-frame (move + floor), 1 for a standing one, 3 with a slide, up to 6 on a step up.
 *
 * Frame of reference: Position in the store is the capsule's bottom (the feet), Up is minus the gravity direction at the feet, and the capsule's axis is aligned to Up, so a gravity change (wall, planet) is followed without special cases.
 * Walkability tests the hit's ImpactNormal (the surface hit, as the character movement does), sliding uses the sweep Normal.
 *
 * Separation: a spatial hash of the slots of the frame (cells of twice the largest radius, sorted by cell then slot) gives each
 * slot a push on the ground plane away from overlapping neighbours, computed from the start-of-frame positions before any slot
 * moves, so the result does not depend on the loop order. The push is a velocity, so the sweep still keeps it out of walls.
 *
 * Body collision (pushbox P5): with Ctx.Bodies, a second hash of those bodies (cells of twice the largest radius, sorted by cell then
 * entity id) gives each slot its overlapping neighbours at the start of the frame. The fighters' soft push is added to the frame's move
 * (minion-minion softness stays the separation above), then FGSTN_Pushbox::ConstrainMove (approach clamp + rigid core) corrects the move
 * against every neighbour, fighters and minions, exactly as a fighter's own move is corrected; the sweep runs after, so walls win.
 * Cost: one sorted hash per frame (n log n) and a 3x3 cell lookup per slot, no pair loop over the whole crowd.
 */
class GSTNMINIONRUNTIME_API FGSTN_MinionMovement_Lightweight : public IGSTN_MinionMovementBackend
{
public:
	virtual void StepBatch(FGSTN_MinionMoveContext& Ctx, TArrayView<const int32> Slots) override;

private:
	/** Per-slot data of one step, kept on the stack */
	struct FStepState
	{
		FVector Centre;
		FVector Up;
		FQuat CapsuleRotation;
		float Radius = 0.f;
		float HalfHeight = 0.f;
		float StepHeight = 0.f;
		float WalkableUpDot = 0.f;
		bool bGrounded = false;
	};

	void ComputeSeparation(const FGSTN_MinionMoveContext& Ctx, TArrayView<const int32> Slots);
	void StepSlot(FGSTN_MinionMoveContext& Ctx, int32 Slot, const UGSTN_MinionDefinition& Definition, const FVector2D& Separation);

	/** Hashes Ctx.Bodies (BodyCells, BodyCellSize) */
	void BuildBodyHash(const FGSTN_MinionMoveContext& Ctx);
	/** The body of Slot in Ctx.Bodies (null when it has none) and its overlapping neighbours into BodyNeighbours, by entity id */
	const FGSTN_PushboxBody* GatherBodyNeighbours(const FGSTN_MinionMoveContext& Ctx, int32 Slot);

	/** Capsule sweep of the slot's shape (RadiusOverride > 0: a thinner capsule with the same bottom, for the floor's perch probe) */
	bool Sweep(const FGSTN_MinionMoveContext& Ctx, const FStepState& State, const FVector& From, const FVector& To, FHitResult& OutHit, float RadiusOverride = 0.f) const;
	/** Pushes the capsule out of whatever it starts inside (up to a few iterations). True when it moved */
	bool Depenetrate(const FGSTN_MinionMoveContext& Ctx, FStepState& State) const;
	/** Sweep + slide (two sweeps max), with the step up attempt for a walking capsule. True when something blocked (OutFirstHit) */
	bool MoveWithCollision(const FGSTN_MinionMoveContext& Ctx, FStepState& State, const FVector& Delta, bool bAllowSlide, bool bAllowStepUp, FHitResult& OutFirstHit);
	bool TryStepUp(const FGSTN_MinionMoveContext& Ctx, FStepState& State, const FVector& Remainder, const FHitResult& BlockingHit) const;
	/**
	 * Down sweep within Distance. True with OutHit on a walkable floor. A contact on the capsule's rim (an edge under the side of
	 * the capsule, as at a ledge or a step down) only counts when a thinner probe (half the radius) also finds walkable ground
	 * within Distance under the axis: a step down stays grounded, a ledge higher than the reach does not
	 */
	bool FindFloor(const FGSTN_MinionMoveContext& Ctx, FStepState& State, float Distance, FHitResult& OutHit) const;

	bool IsWalkable(const FStepState& State, const FHitResult& Hit) const;

	/** Separation velocity by slot (scratch, Capacity entries) */
	TArray<FVector2D> SeparationBySlot;
	/** (cell key, slot) sorted, scratch */
	TArray<TPair<uint64, int32>> Cells;
	/** (cell key, index in Ctx.Bodies) sorted, scratch */
	TArray<TPair<uint64, int32>> BodyCells;
	double BodyCellSize = 1.0;
	/** One slot's neighbours (scratch) and the fighters among them */
	TArray<FGSTN_PushboxBody> BodyNeighbours;
	TArray<FGSTN_PushboxBody> FighterNeighbours;
};
