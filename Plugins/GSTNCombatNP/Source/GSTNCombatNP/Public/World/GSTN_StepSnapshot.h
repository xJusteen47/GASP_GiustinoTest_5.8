// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Hits/GSTN_HurtboxTypes.h"
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
 * the record and a rewind hands the step the extension of the rewound frame. An entry holds one blob per registered contributor
 * (FGSTN_SnapshotExtensionSet, two at most since M6 F7); the victim view exposes each by type id.
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
 * The blobs of every registered contributor for one entity, in ascending type id order (M6 F7: the parts and the defence plugin both
 * contribute). A fixed array of MaxBlobs, trivially copyable like the blob, so the history record and the victim drain copy it as they
 * copied the single blob. Set and Get take the type id, so a contributor never sees another's bytes; GetData keeps the single-pointer
 * reading of the victim view (the lowest id) for a step that validates the type word itself.
 */
struct GSTNCOMBATNP_API FGSTN_SnapshotExtensionSet
{
	static constexpr int32 MaxBlobs = 2;

	FGSTN_SnapshotExtensionBlob Blobs[MaxBlobs];

	/** Any blob set */
	bool IsSet() const { return Blobs[0].IsSet() || Blobs[1].IsSet(); }

	int32 Num() const { return (Blobs[0].IsSet() ? 1 : 0) + (Blobs[1].IsSet() ? 1 : 0); }

	void Reset()
	{
		Blobs[0].Reset();
		Blobs[1].Reset();
	}

	/** The blob of a type id, or null */
	const FGSTN_SnapshotExtensionBlob* Find(uint8 InTypeId) const
	{
		for (const FGSTN_SnapshotExtensionBlob& Blob : Blobs)
		{
			if (Blob.IsSet() && Blob.TypeId == InTypeId)
			{
				return &Blob;
			}
		}
		return nullptr;
	}

	/**
	 * The slot a type id is written into: its own when it has one, else a free slot kept so that the ids stay ascending (the same slot
	 * on every machine, whatever the registration order). With both slots taken by other ids the last one is reused: the registry
	 * refuses a third contributor, so that never happens with registered ids
	 */
	FGSTN_SnapshotExtensionBlob& SlotFor(uint8 InTypeId)
	{
		if (Blobs[0].IsSet() && Blobs[0].TypeId == InTypeId)
		{
			return Blobs[0];
		}
		if (Blobs[1].IsSet() && Blobs[1].TypeId == InTypeId)
		{
			return Blobs[1];
		}
		if (!Blobs[0].IsSet())
		{
			return Blobs[0];
		}
		if (InTypeId < Blobs[0].TypeId)
		{
			Blobs[1] = Blobs[0];
			Blobs[0].Reset();
			return Blobs[0];
		}
		return Blobs[1];
	}

	/** Writes the blob of InTypeId from Value; what the slot held of another id is replaced */
	template <typename ValueT>
	void Set(uint8 InTypeId, const ValueT& Value)
	{
		SlotFor(InTypeId).Set(InTypeId, Value);
	}

	/** Copies a whole blob into the slot of its type id; an empty blob removes the id's slot and packs the other down */
	void Assign(uint8 InTypeId, const FGSTN_SnapshotExtensionBlob& Blob)
	{
		if (!Blob.IsSet())
		{
			if (Blobs[0].IsSet() && Blobs[0].TypeId == InTypeId)
			{
				Blobs[0] = Blobs[1];
				Blobs[1].Reset();
			}
			else if (Blobs[1].IsSet() && Blobs[1].TypeId == InTypeId)
			{
				Blobs[1].Reset();
			}
			return;
		}
		SlotFor(InTypeId) = Blob;
	}

	template <typename ValueT>
	const ValueT* Get(uint8 InTypeId) const
	{
		const FGSTN_SnapshotExtensionBlob* Blob = Find(InTypeId);
		return Blob ? Blob->Get<ValueT>(InTypeId) : nullptr;
	}

	/** The bytes of InTypeId's blob, or null */
	const void* GetData(uint8 InTypeId) const
	{
		const FGSTN_SnapshotExtensionBlob* Blob = Find(InTypeId);
		return Blob ? Blob->GetData() : nullptr;
	}

	/** The bytes of the lowest type id, or null: what FGSTN_HitVictimView::SnapshotExtension points at (a reader validates the type word) */
	const void* GetData() const
	{
		return Blobs[0].GetData();
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

	/** The data of the optional plugins that contribute to the snapshot (empty when none does), one blob per contributor; see FGSTN_SnapshotContributorRegistry */
	FGSTN_SnapshotExtensionSet Extension;

	/**
	 * Hurtboxes plugins add on top of Hurtboxes for this snapshot (a shield bubble), at most GSTNHurtboxIndex::MaxExtraHurtboxes. They are a value of
	 * the end of the previous frame like the rest of the entry, and the lag compensation history keeps them with the record. An entity with no
	 * Hurtboxes set cannot be hit, whatever it carries here
	 */
	FGSTN_ExtraHurtboxList ExtraHurtboxes;

	/** The shape of the hurtbox a hit carries the index of: a set hurtbox, or an extra from GSTNHurtboxIndex::ExtraIndexBase. Null for an index that is neither */
	const FGSTN_HitShape* FindHurtboxShape(int32 HurtboxIndex) const
	{
		if (GSTNHurtboxIndex::IsExtra(HurtboxIndex))
		{
			const int32 Position = GSTNHurtboxIndex::ToExtraPosition(HurtboxIndex);
			return ExtraHurtboxes.IsValidIndex(Position) ? &ExtraHurtboxes[Position].Shape : nullptr;
		}
		return Hurtboxes && Hurtboxes->Hurtboxes.IsValidIndex(HurtboxIndex) ? &Hurtboxes->Hurtboxes[HurtboxIndex].Shape : nullptr;
	}
};

namespace GSTNExtraHurtboxes
{
	/** The radius around the entity's root that holds every extra (its offset plus its largest world extent), for the broadphase */
	GSTNCOMBATNP_API double ComputeBoundRadius(TConstArrayView<FGSTN_ExtraHurtbox> Extras);
}

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

	/**
	 * The entity's action state: the end of the previous frame at the snapshot build, the frame being written at a rebuild in the victim drain
	 * (a contributor whose data exists only outside a hit reaction reads it; M6 F7). Actionable in a test that sets nothing
	 */
	EGSTN_ActionState ActionState = EGSTN_ActionState::Actionable;
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

	/**
	 * Appends the extra hurtboxes the entity has this frame (a shield bubble from the replicated shield state), computed from the context
	 * only, so the attacker's machine, the authority and a lag compensation rewind of the history record give the same shapes. Called by the
	 * snapshot build after Contribute, whether or not the blob is set; what it appends beyond GSTNHurtboxIndex::MaxExtraHurtboxes is dropped. Not called at a rebuild
	 */
	virtual void AppendExtraHurtboxes(const FGSTN_SnapshotContributorContext& Context, FGSTN_ExtraHurtboxList& Out) const {}
};

/**
 * The registered contributors. Contributors are kept sorted by type id, so the order never depends on the plugin load order; the
 * entry and the victim view hold one blob per contributor (FGSTN_SnapshotExtensionSet::MaxBlobs), so a contributor past that is
 * refused (an ensure and false). Game thread only, registered when the module starts and removed when it shuts down.
 */
class GSTNCOMBATNP_API FGSTN_SnapshotContributorRegistry
{
public:
	/** Contributors the snapshot supports at once */
	static constexpr int32 MaxContributors = FGSTN_SnapshotExtensionSet::MaxBlobs;

	static FGSTN_SnapshotContributorRegistry& Get();

	bool Register(IGSTN_SnapshotContributor* Contributor);
	void Unregister(IGSTN_SnapshotContributor* Contributor);

	int32 Num() const { return Contributors.Num(); }

	/** The snapshot build: fills Entry.Extension (one blob per contributor), lets each contributor modify the entry and collects their extra hurtboxes. Resets both first; neutral with nothing registered */
	void Fill(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotEntry& Entry) const;

	/**
	 * The victim drain: rebuilds every contributor's blob of Set from the fragments as they stand now, each contributor seeing its own blob
	 * as Context.Previous (the caller's Previous is ignored). Neutral with nothing registered
	 */
	void Rebuild(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionSet& Set) const;

private:
	TArray<IGSTN_SnapshotContributor*> Contributors;
};
