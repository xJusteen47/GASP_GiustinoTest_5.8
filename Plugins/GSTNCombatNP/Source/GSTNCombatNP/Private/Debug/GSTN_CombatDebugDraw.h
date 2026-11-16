// Combat simulation (GAS-NPP fork): debug draw of the hit geometry. Presentation only: nothing here is read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatAttacker.h"

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

	void DrawHitboxes(const UWorld* World, const TArray<FGSTN_AttackerDebugShape>& Shapes, bool bFaded = false);
	void DrawHurtboxes(const UWorld* World, const FGSTN_SnapshotEntry& Entry);

	/** gstn.Combat.Debug.Body: the real movement capsule, the pushbox cylinder and its rigid core, apart from hit/hurt boxes */
	bool IsBodyDrawOn();
	void DrawBody(const UWorld* World, const FGSTN_SnapshotEntry* Entry, const UCapsuleComponent* MovementCapsule);
}
