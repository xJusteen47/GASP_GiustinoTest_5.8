// Minion runtime (replication, M8 G6).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Net/GSTN_MinionNetRates.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GSTN_MinionPack.generated.h"

class AGSTN_MinionPack;
struct FGSTN_MinionNetArray;

/**
 * One minion on the wire (combat-m8-minions.md 8.2). The FastArray item only carries the quantized wire form; NetSerialize forwards
 * to the Core codec (FGSTN_MinionNetCodec), so the bit layout is the one the Core tests pin down and nothing here re-encodes it.
 *
 * The type travels with every update in v1 (bHasType is always set by the server): a FastArray item cannot tell an add from a change
 * per connection, and a late joiner or a lost add must still learn the type. 9 bits per update, inside the 20 B planning size; the
 * "type on add only" of 8.2 needs a per-connection baseline (Iris or a custom NetDeltaSerialize) and waits for R1's numbers.
 *
 * Client side the callbacks only copy the wire item into the world's UGSTN_MinionClientRegistry; no visuals (G7 reads the registry).
 */
USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_MinionNetItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FGSTN_MinionNetWireItem Wire;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	void PostReplicatedAdd(const FGSTN_MinionNetArray& Array);
	void PostReplicatedChange(const FGSTN_MinionNetArray& Array);
	void PreReplicatedRemove(const FGSTN_MinionNetArray& Array);
};

template<>
struct TStructOpsTypeTraits<FGSTN_MinionNetItem> : public TStructOpsTypeTraitsBase2<FGSTN_MinionNetItem>
{
	enum
	{
		WithNetSerializer = true
	};
};

/** The items of one pack. Mutated only through UGSTN_MinionCombatSubsystem::MutatePack (dormancy order, dirty marks) */
USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_MinionNetArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGSTN_MinionNetItem> Items;

	/** The pack that holds this array (set by the pack, never replicated): the client callbacks need its pack id and world */
	AGSTN_MinionPack* OwnerPack = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FGSTN_MinionNetItem, FGSTN_MinionNetArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FGSTN_MinionNetArray> : public TStructOpsTypeTraitsBase2<FGSTN_MinionNetArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/**
 * Up to 16 minions as one replicated actor (8.1). Minions are not actors on clients; a pack is the unit of relevancy, dormancy and
 * update frequency, so 40 enemies cost three to five actor channels instead of forty.
 *
 * Authority: the minion subsystem spawns packs, assigns members by spawn group at spawn only (no re-packing), sets the update
 * frequency to the best member tier's rate (a ceiling: each item is dirtied at its own tier's rate) and puts an all-T3 pack to sleep
 * with DORM_DormantAll. Relevancy is per connection and budgeted: IsNetRelevantFor asks the subsystem's grants. NetPriority is low
 * (fighters win the bandwidth), bAlwaysRelevant is false and the cull distance is not used (the grants replace it).
 *
 * Push model ready: the item array is a push-based property marked dirty together with MarkItemDirty, so with
 * net.IsPushModelEnabled the pack costs no property compare per update; with push model off the marks are no-ops and the
 * FastArray's own replication keys still drive what is sent.
 */
UCLASS(NotPlaceable, Transient)
class GSTNMINIONRUNTIME_API AGSTN_MinionPack : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxItems = 16;

	AGSTN_MinionPack();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	int32 GetPackId() const { return PackId; }
	const FGSTN_MinionNetArray& GetNetArray() const { return Items; }

	/** Array index of the item with this pack-local id, INDEX_NONE when absent */
	int32 FindItemIndex(uint8 ItemId) const;

	// Authority bookkeeping (never replicated; the subsystem owns its meaning)

	int32 SpawnGroup = 0;
	/** Store slot of each pack-local item id, INDEX_NONE for a free id */
	int32 MemberSlot[MaxItems];
	int32 NumMembers = 0;
	/** Current ceiling: the best member tier rate in Hz, 0 = all T3 (dormant) */
	int32 CeilingHz = 0;
	/** Gates the frames the pack sends in at CeilingHz, and each item at its own tier's rate (GSTNMinionNetRate::SelectPackSends) */
	FGSTN_MinionNetRateLimiter PackLimiter;
	FGSTN_MinionNetRateLimiter ItemLimiters[MaxItems];

private:
	friend class UGSTN_MinionCombatSubsystem;

	/** Authority: the pack id, set once before the first replication (the client builds global ids PackId << 4 | ItemId from it) */
	void InitPack(int32 InPackId, int32 InSpawnGroup);

	/** Push model: marks the item array dirty (a no-op with push model off). Called by MutatePack after MarkItemDirty */
	void MarkItemsPropertyDirty();

	FGSTN_MinionNetArray& GetMutableNetArray() { return Items; }

	UPROPERTY(Replicated)
	int32 PackId = INDEX_NONE;

	UPROPERTY(Replicated)
	FGSTN_MinionNetArray Items;
};
