// gstn.Combat.Debug.BakeCheck: baked socket data against the live mesh socket (combat-m5-editor.md 10.3, 15; plan E16).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_CombatBakeCheck.generated.h"

/** How far a baked socket is from the live one, both in world space */
struct FGSTN_BakeCheckResult
{
	FTransform BakedWorld;
	float DistanceCm = 0.f;
	float AngleDeg = 0.f;
};

namespace GSTNCombatBakeCheck
{
	/**
	 * The baked socket placed the way the attacker step places it (baked root-relative transform * the pawn's root), compared with
	 * the live socket of the mesh. Pure, for the tests
	 */
	FGSTN_BakeCheckResult Compare(const FTransform& BakedRootRelative, const FTransform& Root, const FTransform& LiveWorld);
}

/**
 * Draws, for every pawn running a timeline with baked sockets on a machine that simulates it, each baked socket (cyan) next to the
 * live mesh socket of the same name (magenta), a line between them and the distance. The bake is the editor preview's pose (E6/E7:
 * the preview places the mesh by its bounds, not by the character's real mesh offset), the hitboxes follow it, so a gap here is a gap
 * between where the hitbox is and where the weapon is seen. 1 = draw, 2 = also log one line per socket and frame.
 *
 * Presentation only: reads the latest sync state and the actor, never the simulation. Development builds only (the module is denied
 * in Shipping). A simulated proxy is skipped: it does not run the attacker step, so its baked data says nothing about its hits.
 */
UCLASS()
class UGSTN_CombatBakeCheck : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual ETickableTickType GetTickableTickType() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
