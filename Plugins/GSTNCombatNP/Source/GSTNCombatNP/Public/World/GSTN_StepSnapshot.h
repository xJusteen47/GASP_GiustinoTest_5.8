// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_HitInbox.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "GSTN_StepSnapshot.generated.h"

class UGSTN_HurtboxSetDefinition;

/**
 * Base of the per-entity data an optional plugin adds to the snapshot (broken parts, a guard state; combat-m4-hits 3b item 7).
 * Plain data: a derived struct is copied byte for byte into the entry and held by the lag compensation history, so it holds no
 * pointer, no tag and no container, and stays a few bytes.
 */
USTRUCT(meta = (Hidden))
struct GSTNCOMBATNP_API FGSTN_SnapshotExtension
{
	GENERATED_BODY()

	virtual ~FGSTN_SnapshotExtension() = default;
};

/**
 * One hittable entity as the combat tick of a step sees it: the state at the end of the previous frame (combat.md 2, fact 8),
 * so no attacker's order within the step changes what another reads. An interpolated proxy, which never ticks here, contributes
 * the presentation state it shows, which is what a predicting attacker hits on this machine.
 */
struct GSTNCOMBATNP_API FGSTN_SnapshotEntry
{
	/** The NP proxy id (or the id a provider gives its entities, in its own range) */
	int32 EntityId = INDEX_NONE;

	FGenericTeamId Team;

	/** Where the entity was at the end of the previous frame, from the Mover sim state tracker (the actor's own transform for a proxy) */
	FTransform Transform;

	/** World space velocity at the end of the previous frame */
	FVector Velocity = FVector::ZeroVector;

	/** The unit forward axis of the entity */
	FVector Facing = FVector::ForwardVector;

	/** The direction Mover considers up: minus the gravity direction. Every "up" of the hit math is this, never world Z */
	FVector Up = FVector::UpVector;

	/** The hurtboxes this entity is hit through, or null (an entity with none cannot be hit). Valid for the frame of the snapshot only */
	const UGSTN_HurtboxSetDefinition* Hurtboxes = nullptr;

	/** How each hurtbox of the set takes hits this frame (intangible, invincible, armor), in set order. Derived from the combat state, never stored in it */
	TArray<FGSTN_HurtboxFrameState> HurtStates;

	/** A sphere around Transform that holds every hurtbox, for the broadphase */
	float BoundRadius = 0.f;

	/** In its hitstop window this frame: the one thing an attacker must not feed on */
	bool bInHitstop = false;

	EGSTN_ActionState ActionState = EGSTN_ActionState::Actionable;

	/** The compact state the attacker needs to compute the hitstop the authority computes (percent, weight, flags, threshold) */
	FGSTN_VictimReactionState Reaction;

	/** 1 = full health, from the health attribute set (1 without one). Only the Health damage mode reads it */
	float HealthFraction = 1.f;

	/** Data of optional plugins, each a struct derived from FGSTN_SnapshotExtension. Empty by default */
	TArray<FInstancedStruct> Extensions;
};

/**
 * The hittable entities of one simulation step, sorted by entity id, with a uniform grid for the broadphase. Built by the first
 * combat tick of the frame (UGSTN_CombatWorldSubsystem::EnsureSnapshot) and read by every attacker of the step.
 */
struct GSTNCOMBATNP_API FGSTN_StepSnapshot
{
	/** The frame the snapshot was built for, INDEX_NONE before the first build */
	int32 Frame = INDEX_NONE;

	/** Sorted by EntityId, ascending */
	TArray<FGSTN_SnapshotEntry> Entries;

	/** Edge of a grid cell: twice the largest hurtbox extent of the entries (the default when none has hurtboxes) */
	double CellSize = 400.0;

	/** The entry with this id, or null */
	const FGSTN_SnapshotEntry* Find(int32 EntityId) const;

	/** The entries whose bounding sphere touches Box, as indices into Entries in ascending order (so by entity id) */
	void QueryCandidates(const FBox& Box, TArray<int32>& OutEntryIndices) const;

	/** Builds the grid from Entries (sorted by the caller) */
	void BuildGrid(double DefaultCellSize);

private:
	/** Cell to the entry indices whose bounds overlap it. Only looked up by key, never iterated, so its order cannot reach the simulation */
	TMap<FIntVector, TArray<int32>> Cells;

	FIntVector CellOf(const FVector& Point) const;
};

/**
 * Where a registered entity reads its snapshot entry from. The combat component implements it; a test or a plugin may implement
 * it to put a stand-in entity in the world registry. Called by the snapshot build on the game thread, inside the first combat tick of
 * a step, so it reads end-of-previous-frame state and nothing else.
 */
class GSTNCOMBATNP_API IGSTN_SnapshotSource
{
public:
	virtual ~IGSTN_SnapshotSource() = default;

	/** Fills Out for the snapshot of Frame (EntityId, Team and Hurtboxes are set already). False leaves the entity out of the step */
	virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) = 0;

	/** Delivers a hit to this entity's inbox (combat-m4-hits 7). False when the source takes none (the default) or does not tick on this machine */
	virtual bool PostHit(const FGSTN_HitInboxEntry& Entry) { return false; }
};

/**
 * A plugin that adds entities to the snapshot that are not combat components (minions of M8, a training dummy). Registered with the
 * world subsystem; it is asked after the combat components, in ascending provider id.
 */
class GSTNCOMBATNP_API IGSTN_HittableProvider
{
public:
	virtual ~IGSTN_HittableProvider() = default;

	/** Unique among providers and fixed for the lifetime of the registration: the order providers append in, identical on every machine whatever the load order */
	virtual int32 GetProviderId() const = 0;

	/** Appends the provider's entities for the snapshot of Frame. The caller sorts the whole snapshot by entity id afterwards */
	virtual void AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries) = 0;
};
