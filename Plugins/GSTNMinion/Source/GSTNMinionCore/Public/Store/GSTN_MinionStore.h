// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"
#include "Intent/GSTN_MinionIntent.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Sync/GSTN_EntityRef.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_MinionStore.generated.h"

/**
 * Action state of a minion: the 4-bit wire value of the net item (combat-m8-minions.md 8.2). The order is the wire order and
 * is fixed once shipped; 12 of the 16 values are used. PushedBack, Hitstun and Launched are the three reaction tiers of a hit,
 * so the tier costs no extra bits. GSTNMinionTags::GetActionStateTag names a value for queries.
 */
UENUM()
enum class EGSTN_MinionActionState : uint8
{
	Idle = 0,
	Walk,
	Move,
	Hitstop,
	Hitstun,
	Launched,
	Knockdown,
	GetUp,
	Dying,
	Dead,
	Spawning,
	PushedBack,
	Count UMETA(Hidden)
};

/**
 * Names a live minion. The index is the store slot; the generation tells a recycled slot from the minion that used to be there,
 * so a hit stamped for a dead minion, a stale token or an old AI target is dropped on delivery instead of landing on the new
 * occupant. A live slot's generation is never 0, so the default value (generation 0) is the null handle and never resolves.
 * Reflected (G3) as an opaque Blueprint value: Blueprint passes it back to the spawner and the component, never reads its fields.
 */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionHandle
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 Index = 0;

	UPROPERTY()
	uint16 Generation = 0;

	FGSTN_MinionHandle() = default;
	FGSTN_MinionHandle(uint16 InIndex, uint16 InGeneration) : Index(InIndex), Generation(InGeneration) {}

	/** True for any handle that was ever issued; says nothing about whether its slot is still alive (ask the store) */
	bool IsNull() const { return Generation == 0; }

	bool operator==(const FGSTN_MinionHandle& Other) const { return Index == Other.Index && Generation == Other.Generation; }
	bool operator!=(const FGSTN_MinionHandle& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FGSTN_MinionHandle& Handle) { return (static_cast<uint32>(Handle.Index) << 16) | Handle.Generation; }

	FString ToString() const;
};

namespace GSTNMinion
{
	/** The pipeline id of a minion in the hit pipeline (HitTargets ring, inbox): MinionEntityBase | Index. Opaque to the pipeline */
	inline int32 ToEntityId(const FGSTN_MinionHandle& Handle)
	{
		return GSTNEntityKinds::MakeMinionEntityId(Handle.Index);
	}

	/** The reference stored in a sync state or a link: Kind Minion, the slot index and the low 8 bits of the generation */
	inline FGSTN_EntityRef ToEntityRef(const FGSTN_MinionHandle& Handle)
	{
		return FGSTN_EntityRef(GSTNEntityKinds::Minion, Handle.Index, static_cast<uint8>(Handle.Generation & 0xFF));
	}

	/**
	 * Bits of FGSTN_MinionStore::ExtFlags. Authority-side, never on the wire (the 3 wire Flags stay as they are): they are the switches a
	 * bridge plugin (platforms, parts) flips on a slot, all inert at 0
	 */
	namespace ExtFlag
	{
		/** The minion's collider is carried by a bridge: its pack item is not published (the store keeps simulating it) */
		constexpr uint8 PlatformActor = 1 << 0;
		/** Exempt from the MaxActiveCombat demotion (a platform minion that a player stands on must stay in Combat) */
		constexpr uint8 ExemptFromCombatCap = 1 << 1;
	}

	/** Default of the MaxLODTier column: the farthest tier, i.e. no ceiling */
	constexpr uint8 NoLODCeiling = static_cast<uint8>(EGSTN_MinionLODTier::Dormant);

	/** Slot count a store gets from the console variable gstn.Minion.Max (default 128), clamped to [1, GSTNEntityKinds::MaxMinionIndex + 1] */
	GSTNMINIONCORE_API int32 GetConfiguredCapacity();
}

/**
 * The minion store: a struct of arrays, one column per field, one slot per minion (M8 4c, section 3). The batch stages walk the
 * columns, never an object per minion, which is what makes 40 to 150 minions cheap.
 *
 * Determinism: allocation always takes the lowest free index, so the same sequence of spawns and despawns gives the same
 * handles on every run (the schedule, the token grant order and the replay depend on it). There is no hidden state beyond the
 * columns and the per-slot generation. Freeing a slot bumps its generation (skipping 0 on wrap) and resets the columns, so a
 * stale handle fails IsValid and a recycled slot starts from defaults.
 *
 * The capacity is fixed at Reset: columns never reallocate while the batch runs, so references into them stay valid for a tick.
 * Plain data, no UObject, no world: the same type serves the authority batch and the client puppet tables.
 */
struct GSTNMINIONCORE_API FGSTN_MinionStore
{
	/** Frees everything and sizes the columns to InCapacity slots (clamped as GetConfiguredCapacity does). Generations restart at 1 */
	void Reset(int32 InCapacity);

	/** Reset(GetConfiguredCapacity()) */
	void ResetToConfiguredCapacity();

	int32 Capacity() const { return Generation.Num(); }

	/** Number of live minions */
	int32 Num() const { return NumAlive; }

	bool IsFull() const { return NumAlive >= Capacity(); }

	/** Takes the lowest free slot. Returns the null handle when the store is full */
	FGSTN_MinionHandle Allocate();

	/** Frees the slot of a valid handle; returns false (and changes nothing) for a stale or null handle */
	bool Free(const FGSTN_MinionHandle& Handle);

	/** True when the handle was issued by this store and its slot still holds that minion */
	bool IsValid(const FGSTN_MinionHandle& Handle) const;

	/** True when the slot index holds a live minion, whatever its generation */
	bool IsSlotAlive(int32 Index) const { return Alive.IsValidIndex(Index) && Alive[Index]; }

	/** The handle of a live slot, or the null handle for a free or out-of-range slot */
	FGSTN_MinionHandle HandleOfSlot(int32 Index) const;

	/** The handle of the live slot a minion pipeline id points at (the id carries no generation), or null */
	FGSTN_MinionHandle HandleFromEntityId(int32 EntityId) const;

	/** The first live slot at or after FromIndex, INDEX_NONE if none. Walk the store with it in ascending slot order */
	int32 NextAliveSlot(int32 FromIndex) const;

	// Columns. All have Capacity() entries; a column of a free slot holds its defaults.

	/** Per-slot generation, never 0 once issued; the single source of truth of IsValid */
	TArray<uint16> Generation;
	TArray<FVector3f> Position;
	TArray<float> YawDegrees;
	TArray<float> Speed;
	TArray<float> Health;
	TArray<float> MaxHealth;
	/** Index into the project registry of definitions (same order on both sides of the wire) */
	TArray<uint8> TypeIndex;
	TArray<uint8> ActionState;
	/** Move or reaction timeline index of the type's list, 0 = none (6 bits on the wire) */
	TArray<uint8> MoveId;
	TArray<int32> MoveStartFrame;
	TArray<uint8> HitCounter;
	/** Net item flags (Teleport, Promoting, Elite); see GSTNMinion::NetFlags in Net/GSTN_MinionNetCodec.h */
	TArray<uint8> Flags;
	/** EGSTN_MinionLODTier of the slot and the frames it has spent in it (dwell of the LOD hysteresis) */
	TArray<uint8> LODTier;
	TArray<int32> FramesInTier;
	TArray<FGSTN_MinionIntent> Intent;
	/** GSTNMinion::ExtFlag bits (bridge switches, authority only, never on the wire) */
	TArray<uint8> ExtFlags;
	/**
	 * Per-slot LOD ceiling (G10 seam): the farthest EGSTN_MinionLODTier the slot may be in. GSTNMinion::NoLODCeiling (the farthest tier)
	 * means no constraint. A bridge lowers it while a player is based on the minion or within mount range
	 */
	TArray<uint8> MaxLODTier;
	/** Extension byte of the slot's net item, published only for types with UGSTN_MinionDefinition::bHasNetExt (owned by a bridge) */
	TArray<uint8> ExtByte;

private:
	void ResetSlot(int32 Index);

	TBitArray<> Alive;
	int32 NumAlive = 0;
};
