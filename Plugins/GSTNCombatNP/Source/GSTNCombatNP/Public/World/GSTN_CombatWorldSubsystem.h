// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/GSTN_KOZone.h"
#include "World/GSTN_StepSnapshot.h"
#include "GSTN_CombatWorldSubsystem.generated.h"

class UGSTN_HurtboxSetDefinition;
class AGSTN_KOZone;

/**
 * One entity at the end of one simulated frame, as the history ring of the world subsystem keeps it for lag compensation (combat-m4-hits 9).
 * Plain data, a few dozen bytes: the reaction state is the compact one the attacker's hitstop needs, so it rewinds with the transform.
 */
struct FGSTN_EntityHistoryRecord
{
	int32 Frame = INDEX_NONE;

	/** Simulation time the state is the end of (the start time of Frame), in the time base of the fork's lag compensation history */
	float SimTimeMs = 0.f;

	FTransform Transform;
	FVector Velocity = FVector::ZeroVector;
	FGSTN_VictimReactionState Reaction;

	/** The plugin extension of the entry (broken parts): a rewind hands the step the one of the rewound frame */
	FGSTN_SnapshotExtensionBlob Extension;
};

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
	const FGSTN_StepSnapshot& EnsureSnapshot(int32 Frame, float SimTimeMs = -1.f, float StepMs = 0.f);

	/**
	 * Turns the per-entity history on and sizes it for at least HistoryMs of rewind. Called by the combat components of an authority whose rules
	 * use lag compensation; a client never records. The depth only grows. With it off (the default) EnsureSnapshot records nothing
	 */
	void RequestHistory(int32 HistoryMs);
	bool IsRecordingHistory() const { return HistoryMs > 0; }

	/** Appends the entities of Snapshot to the ring at (Frame, SimTimeMs); records of this frame or later (a rollback) are dropped first. Public for tests */
	void RecordHistory(const FGSTN_StepSnapshot& Snapshot, float SimTimeMs, float StepMs);

	/**
	 * The record of EntityId at SimTimeMs: the latest one at or before it (the oldest when asked for a time before the ring), so discrete state
	 * reads the step function it was. With bInterpolateTransform the transform and velocity blend towards the next record. False without records
	 */
	bool SampleHistory(int32 EntityId, float SimTimeMs, FGSTN_EntityHistoryRecord& Out, bool bInterpolateTransform) const;

	/** Records held for an entity (diagnostic and tests) */
	int32 GetNumHistoryRecords(int32 EntityId) const;

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

	/**
	 * KO zones (AGSTN_KOZone): static boxes, read once at registration. A combatant whose simulated position is inside one is knocked out by
	 * its combat tick. The transform and extent are copied, so the answer depends on nothing that can move between machines
	 */
	void RegisterKOZone(const AGSTN_KOZone* Zone, const FTransform& Transform, const FVector& HalfExtent);
	void UnregisterKOZone(const AGSTN_KOZone* Zone);
	bool IsInKOZone(const FVector& Location) const;
	int32 GetNumKOZones() const { return KOZones.Num(); }

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

	struct FKOZoneEntry
	{
		TWeakObjectPtr<const AGSTN_KOZone> Zone;
		FGSTN_KOZoneBounds Bounds;
	};
	TArray<FKOZoneEntry> KOZones;

	/** Sorted by provider id */
	TArray<IGSTN_HittableProvider*> Providers;

	/** Per entity id, ascending in frame and time. Never read by the simulation of the entity, only by an attacker's rewound view */
	TMap<int32, TArray<FGSTN_EntityHistoryRecord>> History;
	int32 HistoryMs = 0;

	FGSTN_StepSnapshot Snapshot;
	bool bSnapshotInvalid = true;
	int32 SnapshotBuildCount = 0;
};
