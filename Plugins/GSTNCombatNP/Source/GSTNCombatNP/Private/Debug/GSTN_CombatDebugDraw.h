// Combat simulation (GAS-NPP fork): debug draw of the hit geometry. Presentation only: nothing here is read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatAttacker.h"

class AActor;
class UWorld;
struct FGSTN_SnapshotEntry;
class UCapsuleComponent;

namespace GSTNCombatDebug
{
	/** gstn.Combat.Debug.Hitboxes: the world shapes of the open hitboxes (the attacker step records them while it is on) */
	bool IsHitboxDrawOn();

	/** gstn.Combat.Debug.Hurtboxes: the hurtboxes of the entities, coloured by how they take hits this frame */
	bool IsHurtboxDrawOn();

	/** gstn.Combat.Debug.HitLog: the attacker step writes the hit log and hitbox trace the harness reads */
	bool IsHitLogOn();

	/** gstn.Combat.Debug.HitboxLinger: seconds the last open hitboxes stay drawn (faded) after they close */
	float GetHitboxLingerSeconds();

	/**
	 * gstn.Combat.Debug.FollowMesh (default 1): where the debug shapes of a fighter are drawn. The simulation places them on the simulated root
	 * (fixed tick, 62.5 Hz); Mover draws the character through a visual mesh smoothed between the last two ticks, so the two are up to a tick
	 * apart (~10 cm at run speed). 1 = drawn on the presented mesh (they stay on the body), 0 = on the simulated root (where the hits are).
	 * Returns the offset to add (presented root - simulated root), zero when off or when the owner has no Mover visual smoothing.
	 */
	FVector GetPresentationOffset(const AActor* Owner);

	/** Offset: GetPresentationOffset of the owner; moves the shapes that ride the owner (Root and BakedSocket spaces), not world-placed ones */
	void DrawHitboxes(const UWorld* World, const TArray<FGSTN_AttackerDebugShape>& Shapes, bool bFaded = false, const FVector& Offset = FVector::ZeroVector);
	void DrawHurtboxes(const UWorld* World, const FGSTN_SnapshotEntry& Entry, const FVector& Offset = FVector::ZeroVector);

	/** gstn.Combat.Debug.Body: the real movement capsule, the pushbox cylinder and its rigid core, apart from hit/hurt boxes */
	bool IsBodyDrawOn();
	void DrawBody(const UWorld* World, const FGSTN_SnapshotEntry* Entry, const UCapsuleComponent* MovementCapsule, const FVector& Offset = FVector::ZeroVector);
}
