// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Simulation/GSTN_CombatActionTypes.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Sync/GSTN_SyncFragment.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "GSTN_StepSnapshot.generated.h"

class AActor;
class UGSTN_HurtboxSetDefinition;

/**
 * Reflected marker base of the snapshot extension types (combat-m4-hits 3b item 7). The data itself travels in
 * FGSTN_SnapshotExtensionBlob, which the entry holds and the history copies; nothing derives from this struct yet.
 */
USTRUCT(meta = (Hidden))
struct GSTNCOMBATNP_API FGSTN_SnapshotExtension
{
	GENERATED_BODY()

	virtual ~FGSTN_SnapshotExtension() = default;
};

/**
 * The per-entity data of ONE optional plugin in the snapshot (combat-m4-hits 3b item 7, spec seam D9): a fixed, trivially copyable
 * buffer with the feature's type id (GSTNFeatureIds). No heap, no tag and no container, so the lag compensation history copies it with
 * the record and a rewind hands the step the extension of the rewound frame. Only one extension type is supported today (the victim
 * view has a single SnapshotExtension pointer): FGSTN_SnapshotContributorRegistry refuses a second contributor.
 *
 * MaxBytes is the smallest size that holds the parts extension (type word, mask, 16 damage bytes, serial, set pointer): 32 B, 40 B
 * with the header. A pointer in it is allowed only for content that outlives every entity (a data asset kept alive by its owner).
 */
struct GSTNCOMBATNP_API FGSTN_SnapshotExtensionBlob
{
	static constexpr int32 MaxBytes = 32;

	/** GSTNFeatureIds of the contributor that wrote it; Invalid (0) = empty */
	uint8 TypeId = 0;

	alignas(8) uint8 Bytes[MaxBytes] = {};

	bool IsSet() const { return TypeId != 0; }

	/** The bytes the victim view points at, or null when empty. Valid as long as this blob is neither changed nor destroyed */
	const void* GetData() const { return TypeId != 0 ? static_cast<const void*>(Bytes) : nullptr; }

	void Reset()
	{
		TypeId = 0;
		FMemory::Memzero(Bytes, MaxBytes);
	}

	template <typename ValueT>
	void Set(uint8 InTypeId, const ValueT& Value)
	{
		static_assert(TIsTriviallyCopyAssignable<ValueT>::Value && sizeof(ValueT) <= MaxBytes && alignof(ValueT) <= 8, "a snapshot extension is a small trivially copyable struct");
		Reset();
		TypeId = InTypeId;
		FMemory::Memcpy(Bytes, &Value, sizeof(ValueT));
	}

	template <typename ValueT>
	const ValueT* Get(uint8 InTypeId) const
	{
		return TypeId == InTypeId && TypeId != 0 ? reinterpret_cast<const ValueT*>(Bytes) : nullptr;
	}
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

	/** The data of the optional plugin that contributes to the snapshot (empty when none does); see FGSTN_SnapshotContributorRegistry */
	FGSTN_SnapshotExtensionBlob Extension;
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

	/** Delivers a message (a hit, or an entry of an open kind) to this entity's inbox (combat-m4-hits 7). False when the source takes none (the default) or does not tick on this machine */
	virtual bool PostToInbox(const FGSTN_InboxEntry& Entry) { return false; }
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

/** What a snapshot contributor reads (spec seam D9). Every member is a value of the end of the previous frame, or constant data */
struct FGSTN_SnapshotContributorContext
{
	/** The frame the snapshot is built for */
	int32 Frame = INDEX_NONE;

	/** The entity's actor, to look up content its plugin attached (never state); null in a test or a rebuild inside the victim drain */
	const AActor* Owner = nullptr;

	/** The entity's sync fragments: end of the previous frame at the snapshot build, the frame being written at a rebuild in the victim drain */
	const FGSTN_SyncFragmentArray* Fragments = nullptr;

	/** The blob this contributor wrote earlier for the entity (a rebuild reuses what it carries, such as a content pointer); null at the first build */
	const FGSTN_SnapshotExtensionBlob* Previous = nullptr;
};

/**
 * Adds one plugin's per-entity data to the snapshot (broken parts, a guard state). C++ only, deterministic, reads the context and nothing
 * else: it runs on every machine for every entity every frame, in the snapshot build, and is called again by the victim drain between
 * two inbox entries.
 */
class GSTNCOMBATNP_API IGSTN_SnapshotContributor
{
public:
	virtual ~IGSTN_SnapshotContributor() = default;

	/** The contributor's feature id from GSTNFeatureIds (the blob's type id). Fixed for the lifetime of the registration */
	virtual uint8 GetTypeId() const = 0;

	/** Fills Blob from the context; leave it reset for an entity that has nothing to contribute */
	virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const = 0;

	/** Lets the contributor change the entry the snapshot build just filled (the hurtbox states of broken parts). Not called at a rebuild */
	virtual void ModifyEntry(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotEntry& Entry) const {}
};

/**
 * The registered contributor. Contributors are kept sorted by type id, so the order never depends on the plugin load order; the
 * victim view carries a single extension pointer today, so a second contributor with another id is refused (an ensure and false).
 * Game thread only, registered when the module starts and removed when it shuts down.
 */
class GSTNCOMBATNP_API FGSTN_SnapshotContributorRegistry
{
public:
	/** Contributors the snapshot supports at once */
	static constexpr int32 MaxContributors = 1;

	static FGSTN_SnapshotContributorRegistry& Get();

	bool Register(IGSTN_SnapshotContributor* Contributor);
	void Unregister(IGSTN_SnapshotContributor* Contributor);

	int32 Num() const { return Contributors.Num(); }

	/** The snapshot build: fills Entry.Extension, then lets the contributor modify the entry. Resets the extension first; neutral with nothing registered */
	void Fill(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotEntry& Entry) const;

	/** The victim drain: rebuilds Blob from the fragments as they stand now. Neutral with nothing registered */
	void Rebuild(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const;

private:
	TArray<IGSTN_SnapshotContributor*> Contributors;
};
