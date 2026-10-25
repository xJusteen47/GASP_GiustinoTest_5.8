// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/GSTN_StepSnapshot.h"
#include "GSTN_CombatWorldSubsystem.generated.h"

class UGSTN_HurtboxSetDefinition;

/**
 * The hittable entities of a world and the per-step snapshot the attackers hit against (combat-m4-hits 5).
 *
 * Registry: one entry per entity, sorted by entity id (the NP proxy id), so every machine walks the same order whatever order the
 * actors began play in. Snapshot: the state of every entity at the end of the previous frame, built by the first combat tick of a
 * frame and read by all of them, which is what makes the result independent of which attacker ticks first (combat.md 2, fact 8).
 * A rollback restores every combat component, and each one's RestoreFrame invalidates the snapshot, so a re-simulated frame never
 * reuses one built from the state before the rollback.
 *
 * Game thread only, like the simulation tick that drives it.
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_CombatWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Cell edge when no entity has hurtboxes: twice a 200 cm largest extent */
	static constexpr double DefaultCellSize = 400.0;

	/** The subsystem of World, null for a null world or one without it */
	static UGSTN_CombatWorldSubsystem* Get(const UWorld* World);

	/**
	 * Adds an entity to the registry. Owner is the object that keeps the source alive (the component): the entry is skipped once it
	 * is gone; null for a source that outlives the registration. @return false when the id is taken or invalid
	 */
	bool RegisterEntity(int32 EntityId, IGSTN_SnapshotSource& Source, UObject* Owner, FGenericTeamId Team, const UGSTN_HurtboxSetDefinition* Hurtboxes);

	void UnregisterEntity(int32 EntityId);

	/** @return false when a provider with the same id is registered */
	bool RegisterProvider(IGSTN_HittableProvider* Provider);
	void UnregisterProvider(IGSTN_HittableProvider* Provider);

	/**
	 * The snapshot of Frame, built now when it is not the one held or when it was invalidated. Called by the first combat tick of
	 * every frame; the others get the same snapshot back. Never call it with a frame older than the held one without an
	 * invalidation in between (a rollback that did not reach RestoreFrame): that is reported in development builds
	 */
	const FGSTN_StepSnapshot& EnsureSnapshot(int32 Frame);

	/** The next EnsureSnapshot builds again. Called by every combat component's RestoreFrame, and by anything that changes an entity out of the tick */
	void InvalidateSnapshot();

	/** Hands a hit to the entity's inbox through its source. False for an unknown id, a source that is gone or one that takes none */
	bool PostHit(int32 EntityId, const FGSTN_HitInboxEntry& Entry);

	/** The snapshot as it is, without building: for presentation and tests */
	const FGSTN_StepSnapshot& GetSnapshot() const { return Snapshot; }

	/** The actor that owns the registered entity (a combat component's owner), or null for a provider's entity or one that is gone. For effect contexts, never for simulation state */
	AActor* FindOwnerActor(int32 EntityId) const;

	/** Snapshots built so far (diagnostic): equals the number of frames simulated plus the rebuilds a rollback forced */
	int32 GetSnapshotBuildCount() const { return SnapshotBuildCount; }

	int32 GetNumRegisteredEntities() const { return Registry.Num(); }

	/** The radius of a sphere around the root that holds every hurtbox of the set, 0 for none */
	static double ComputeBoundRadius(const UGSTN_HurtboxSetDefinition* Hurtboxes);

private:
	struct FRegistryEntry
	{
		int32 EntityId = INDEX_NONE;
		IGSTN_SnapshotSource* Source = nullptr;
		TWeakObjectPtr<UObject> Owner;
		bool bHasOwner = false;
		FGenericTeamId Team;
		TWeakObjectPtr<const UGSTN_HurtboxSetDefinition> Hurtboxes;
	};

	/** Sorted by EntityId */
	TArray<FRegistryEntry> Registry;

	/** Sorted by provider id */
	TArray<IGSTN_HittableProvider*> Providers;

	FGSTN_StepSnapshot Snapshot;
	bool bSnapshotInvalid = true;
	int32 SnapshotBuildCount = 0;
};
