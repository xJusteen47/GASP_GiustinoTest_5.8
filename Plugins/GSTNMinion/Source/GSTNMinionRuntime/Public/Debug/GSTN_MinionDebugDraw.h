// Minion runtime (authority batch, M8 G3; spike 3 scenario, course and robustness counters G4; debug draw G12).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_MinionDebugDraw.generated.h"

namespace GSTNMinionDebug
{
	/**
	 * True when any gstn.Minion.Debug.* variable is on. One flag read: the draw subsystem ticks only while it holds, so every variable
	 * off costs the world tick a single check. Always false in Shipping, where the variables do not exist
	 */
	GSTNMINIONRUNTIME_API bool IsAnyEnabled();

	/** Draw passes done since start (tests: "off = no work" means this does not move) */
	GSTNMINIONRUNTIME_API int32 GetPassCount();
}

/**
 * Debug draw of the minions of the host world (the one whose combat subsystem is active, i.e. the server or the listen host; a client
 * has no store and draws nothing). Not in Shipping. One pass per world tick, only while a variable is on:
 *
 *   gstn.Minion.Debug.Capsules 1  a capsule per live minion, coloured by LOD tier
 *   gstn.Minion.Debug.Hitboxes 1  the reach sphere (MaxRange) of the move a minion is playing; the real hitbox shapes need the timeline
 *                                 evaluated per minion, which a debug pass must not do, so this is the cheap stand-in
 *   gstn.Minion.Debug.LOD      1  the tier as text above each minion
 *   gstn.Minion.Debug.History  N  the last N history poses (lag compensation ring, N up to its depth) as fading points
 *   gstn.Minion.Debug.Intent   1  the intent (direction and speed, green) and the brain waypoint (cyan) as lines
 *
 * Reads the store and the history only; writes nothing, so it cannot change a result. DrawDebug* with lifetime 0 (one frame).
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionDebugSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
};
