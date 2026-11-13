// Minion runtime (replication, M8 G6).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_MinionNetManager.generated.h"

/** What a client needs once to decode every pack item: the type table, its hash and the position origin */
USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_MinionNetRegistryInfo
{
	GENERATED_BODY()

	/** Definition object path by type index, in the server's registration order (the item's TypeIndex indexes this) */
	UPROPERTY()
	TArray<FString> TypePaths;

	/** FGSTN_MinionRegistryHash::Compute of TypePaths on the server (uint32 bits) */
	UPROPERTY()
	int32 Hash = 0;

	/** Registry WorldOrigin (8.2): item positions are offsets from it */
	UPROPERTY()
	FVector WorldOrigin = FVector::ZeroVector;
};

/**
 * One small always-relevant actor per world that carries the minion registry to clients and lets them check it (registry hash at
 * connect). Chosen as the carrier over a value on each pack because a pack may be denied, dormant or absent when a client connects
 * (no minion yet, all far away), while the check and the type table must arrive before the first item is decoded; and because the
 * table changes only when the server registers a new type, so with push model it costs its bytes once per connection plus once
 * per new type, nothing per update (NetUpdateFrequency 1).
 *
 * Client: OnRep resolves every path (a definition that does not load on the client hashes as an empty path), recomputes the hash and
 * compares; a mismatch is an error log and a counter on UGSTN_MinionClientRegistry, never a kick (the check is diagnostic in v1).
 */
UCLASS(NotPlaceable, Transient)
class GSTNMINIONRUNTIME_API AGSTN_MinionNetManager : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_MinionNetManager();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority: replaces the registry info and marks it dirty */
	void SetRegistryInfo(const FGSTN_MinionNetRegistryInfo& InInfo);

	const FGSTN_MinionNetRegistryInfo& GetRegistryInfo() const { return Info; }

private:
	UFUNCTION()
	void OnRep_Info();

	UPROPERTY(ReplicatedUsing = OnRep_Info)
	FGSTN_MinionNetRegistryInfo Info;
};
