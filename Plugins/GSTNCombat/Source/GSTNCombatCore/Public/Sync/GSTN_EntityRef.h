// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_EntityRef.generated.h"

/**
 * A reference to a combat entity that is small enough for the wire and for the sync state: no pointer, no tag, no name. A feature
 * that links two entities (a grab, a held object) stores one of these in its fragment instead of an actor, so the reference means
 * the same on the server, the owning client and an observer.
 *
 * Kind says which table Id indexes (GSTNEntityKinds: the fighter's Network Prediction proxy id, a minion's or an object's own
 * id); Serial tells a reused Id from the entity it used to be, so a stale reference to a destroyed entity does not resolve to
 * the one that took its slot. The serial is owned by the kind's table, never by this struct. The default value is the null
 * reference.
 */
USTRUCT()
struct GSTNCOMBATCORE_API FGSTN_EntityRef
{
	GENERATED_BODY()

	/** The kind of the entity: a GSTNEntityKinds id. GSTNEntityKinds::None is the null reference */
	UPROPERTY(EditAnywhere, Category = "Entity")
	uint8 Kind = 255;

	/** The id inside the kind's table (for a fighter, its combat entity id) */
	UPROPERTY(EditAnywhere, Category = "Entity")
	uint16 Id = 0;

	/** Distinguishes entities that reused the same Id in the kind's table */
	UPROPERTY(EditAnywhere, Category = "Entity")
	uint8 Serial = 0;

	FGSTN_EntityRef() = default;

	FGSTN_EntityRef(uint8 InKind, uint16 InId, uint8 InSerial = 0)
		: Kind(InKind), Id(InId), Serial(InSerial)
	{
	}

	/** The null reference: points at nothing and equals every other null reference, whatever its Id and Serial */
	static FGSTN_EntityRef Null() { return FGSTN_EntityRef(); }

	bool IsNull() const;

	/** Null references are equal to each other; any other pair must match on all three fields */
	bool operator==(const FGSTN_EntityRef& Other) const;
	bool operator!=(const FGSTN_EntityRef& Other) const { return !(*this == Other); }

	/**
	 * Writes or reads the reference in 32 bits (8 kind, 16 id, 8 serial) for a replicated property or a custom serializer. A null
	 * reference is written as the null value, so equal references always have equal bytes
	 */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** "Fighter:12#3", "Minion:5#0", "Kind200:7#1" or "None" */
	FString ToString() const;

	friend uint32 GetTypeHash(const FGSTN_EntityRef& Ref);
};

template <>
struct TStructOpsTypeTraits<FGSTN_EntityRef> : public TStructOpsTypeTraitsBase2<FGSTN_EntityRef>
{
	enum
	{
		WithNetSerializer = true,
		WithIdenticalViaEquality = true
	};
};
