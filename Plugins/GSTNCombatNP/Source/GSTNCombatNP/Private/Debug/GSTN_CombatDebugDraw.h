// Combat simulation (GAS-NPP fork): debug draw of the hit geometry. Presentation only: nothing here is read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatAttacker.h"

class UWorld;
struct FGSTN_SnapshotEntry;

namespace GSTNCombatDebug
{
	/** gstn.Combat.Debug.Hitboxes: the world shapes of the open hitboxes (the attacker step records them while it is on) */
	bool IsHitboxDrawOn();

	/** gstn.Combat.Debug.Hurtboxes: the hurtboxes of the entities, coloured by how they take hits this frame */
	bool IsHurtboxDrawOn();

	/** gstn.Combat.Debug.HitLog: the attacker step writes the hit log and hitbox trace the harness reads */
	bool IsHitLogOn();

	void DrawHitboxes(const UWorld* World, const TArray<FGSTN_AttackerDebugShape>& Shapes);
	void DrawHurtboxes(const UWorld* World, const FGSTN_SnapshotEntry& Entry);
}
