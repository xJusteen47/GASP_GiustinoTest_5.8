// Minion runtime (replication, M8 G6).

#pragma once

#include "CoreMinimal.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_MinionClientRegistry.generated.h"

class AGSTN_MinionPack;
class UGSTN_MinionDefinition;
struct FGSTN_MinionNetRegistryInfo;

/** The latest item a client received for one minion: the wire form as it arrived (G7 dequantizes it with its frame estimate) */
struct GSTNMINIONRUNTIME_API FGSTN_MinionClientItem
{
	int32 PackId = INDEX_NONE;
	uint8 ItemId = 0;
	FGSTN_MinionNetWireItem Wire;
	/** The pack actor it came from: a pack id can be reused after a pack is destroyed, the pointer tells the two apart */
	TWeakObjectPtr<const AGSTN_MinionPack> Pack;
	/** Adds and changes received for this minion */
	int32 Updates = 0;
};

/**
 * The client-side view of the minions (8.4 input): every item of every pack this client currently has, keyed by the global id
 * PackId << 4 | ItemId (8.2), plus the registry the server sent and the result of its hash check. Written only by the pack items'
 * replication callbacks and the net manager's OnRep; no visuals and no simulation here (G7 builds the puppets from it).
 *
 * Exists in every game world; on the authority it stays empty (the callbacks only fire on receivers), so the host's puppets read
 * the store instead (M8 4d.1).
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionClientRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static int32 MakeGlobalId(int32 PackId, uint8 ItemId) { return (PackId << 4) | (ItemId & 0x0F); }

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// Written by the replication callbacks
	void OnItemReceived(const AGSTN_MinionPack& Pack, const FGSTN_MinionNetWireItem& Wire);
	void OnItemRemoved(const AGSTN_MinionPack& Pack, uint8 ItemId);
	/** The pack left this client (denied, destroyed, level end): its items go */
	void OnPackGone(const AGSTN_MinionPack& Pack);

	/** The net manager's info arrived or changed: resolves the type table and checks the hash */
	void ApplyRegistryInfo(const FGSTN_MinionNetRegistryInfo& Info);

	const TMap<int32, FGSTN_MinionClientItem>& GetItems() const { return Items; }
	const FGSTN_MinionClientItem* FindItem(int32 GlobalId) const { return Items.Find(GlobalId); }

	/** Position origin of the items, from the server (zero until the manager arrived) */
	const FVector3f& GetWorldOrigin() const { return WorldOrigin; }

	/** The definition of a type index as this client resolved it; null when unknown or not loadable */
	const UGSTN_MinionDefinition* GetDefinition(int32 TypeIndex) const;

	bool HasCheckedRegistry() const { return RegistryChecks > 0; }
	bool IsRegistryMatching() const { return bRegistryMatching; }
	int32 GetRegistryChecks() const { return RegistryChecks; }
	int32 GetRegistryMismatches() const { return RegistryMismatches; }
	int64 GetItemUpdatesReceived() const { return ItemUpdatesReceived; }

	/** Log lines for the harness (gstn.Minion.Net.Dump): one per item and a summary */
	void DumpToLog() const;

private:
	TMap<int32, FGSTN_MinionClientItem> Items;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_MinionDefinition>> Definitions;

	FVector3f WorldOrigin = FVector3f::ZeroVector;
	uint32 LastServerHash = 0;
	int32 LastTypeCount = INDEX_NONE;
	bool bRegistryMatching = false;
	int32 RegistryChecks = 0;
	int32 RegistryMismatches = 0;
	int64 ItemUpdatesReceived = 0;
};
