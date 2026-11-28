// Single entry point of the collision queries the rollback simulation makes.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"

class UWorld;

/**
 * Every collision query a simulation tick makes (Mover modes and layered moves, combat attacker sweeps, the world sim, minion movement
 * in the rollback, link watchdog...) goes through here instead of calling UWorld directly. Two reasons (physics research 1A, owner
 * 2026-11-25, project files fisica/jolt-box3d-valutazione-2026-11-25.md):
 *  1. One place to swap the backend. Today every call forwards to the Chaos scene, unchanged. If the determinism test shows that Chaos
 *     queries give different answers on different machines, a deterministic backend (Jolt with cross-platform determinism) answers here
 *     and no call site changes.
 *  2. One place to see divergence. With gstn.SimQuery.Hash 1 every query folds its inputs and its answer, bit for bit, into a hash of
 *     the current simulation frame of its world. Two machines that simulated the same frame from the same state must have the same hash;
 *     the first frame where they differ, and the query count, say where to look. The frame is set by whoever ticks first in the frame
 *     (the world sim model, sorted before every pawn model): BeginFrame. A frame simulated again (rollback) starts its hash again.
 *
 * Signatures mirror UWorld's, with the world first. Presentation, camera, animation, editor and debug code keep calling UWorld: their
 * queries never decide the game, and routing them here would only add noise to the hash.
 */
struct GSTNCORE_API FGSTN_SimQuery
{
	// Line traces
	static bool LineTraceTestByChannel(const UWorld* World, const FVector& Start, const FVector& End, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool LineTraceSingleByChannel(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool LineTraceMultiByChannel(const UWorld* World, TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, ECollisionChannel Channel,
		const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam, const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool LineTraceSingleByObjectType(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End,
		const FCollisionObjectQueryParams& ObjectParams, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);

	// Shape sweeps
	static bool SweepTestByChannel(const UWorld* World, const FVector& Start, const FVector& End, const FQuat& Rot, ECollisionChannel Channel,
		const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
		const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool SweepSingleByChannel(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot,
		ECollisionChannel Channel, const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
		const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool SweepMultiByChannel(const UWorld* World, TArray<FHitResult>& OutHits, const FVector& Start, const FVector& End, const FQuat& Rot,
		ECollisionChannel Channel, const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
		const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool SweepSingleByObjectType(const UWorld* World, FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& Rot,
		const FCollisionObjectQueryParams& ObjectParams, const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);

	// Overlaps
	static bool OverlapBlockingTestByChannel(const UWorld* World, const FVector& Pos, const FQuat& Rot, ECollisionChannel Channel,
		const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
		const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool OverlapMultiByChannel(const UWorld* World, TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot,
		ECollisionChannel Channel, const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
		const FCollisionResponseParams& Response = FCollisionResponseParams::DefaultResponseParam);
	static bool OverlapMultiByObjectType(const UWorld* World, TArray<FOverlapResult>& OutOverlaps, const FVector& Pos, const FQuat& Rot,
		const FCollisionObjectQueryParams& ObjectParams, const FCollisionShape& Shape, const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam);

	// Determinism hash (gstn.SimQuery.Hash). Outside the simulation's state: nothing simulated ever reads it

	/** Start the hash of Frame for World (a frame simulated again starts over). Called once per simulation frame, before any query */
	static void BeginFrame(const UWorld* World, int32 Frame);

	/** The hash and query count of Frame for World, if it is still in the ring (the last 256 frames) */
	static bool GetFrameHash(const UWorld* World, int32 Frame, uint32& OutHash, int32& OutQueries);

	/** Whether queries fold into the hash (gstn.SimQuery.Hash) */
	static bool IsHashEnabled();
};
