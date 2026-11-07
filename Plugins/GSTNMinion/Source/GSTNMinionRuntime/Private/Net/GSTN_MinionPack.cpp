// Minion runtime (replication, M8 G6).

#include "Net/GSTN_MinionPack.h"

#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/UnrealNetwork.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"

namespace
{
	/** v1 wire format: no global id field (re-pack fallback off), no extension byte (no bridge sends one yet) */
	const FGSTN_MinionNetFormat& PackItemFormat()
	{
		static const FGSTN_MinionNetFormat Format;
		return Format;
	}

	UGSTN_MinionClientRegistry* RegistryOf(const FGSTN_MinionNetArray& Array)
	{
		const UWorld* World = Array.OwnerPack ? Array.OwnerPack->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UGSTN_MinionClientRegistry>() : nullptr;
	}
}

bool FGSTN_MinionNetItem::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = FGSTN_MinionNetCodec::Serialize(Ar, Wire, PackItemFormat());
	return true;
}

void FGSTN_MinionNetItem::PostReplicatedAdd(const FGSTN_MinionNetArray& Array)
{
	if (UGSTN_MinionClientRegistry* Registry = RegistryOf(Array))
	{
		Registry->OnItemReceived(*Array.OwnerPack, Wire);
	}
}

void FGSTN_MinionNetItem::PostReplicatedChange(const FGSTN_MinionNetArray& Array)
{
	if (UGSTN_MinionClientRegistry* Registry = RegistryOf(Array))
	{
		Registry->OnItemReceived(*Array.OwnerPack, Wire);
	}
}

void FGSTN_MinionNetItem::PreReplicatedRemove(const FGSTN_MinionNetArray& Array)
{
	if (UGSTN_MinionClientRegistry* Registry = RegistryOf(Array))
	{
		Registry->OnItemRemoved(*Array.OwnerPack, Wire.Id);
	}
}

AGSTN_MinionPack::AGSTN_MinionPack()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = false;
	bOnlyRelevantToOwner = false;
	SetReplicateMovement(false);
	// Fighters win the bandwidth; a pack is late by a frame at worst, and its items are interpolated anyway (8.4)
	NetPriority = 0.5f;
	// The subsystem sets the ceiling each frame from the member tiers; this is only the value before the first publish
	SetNetUpdateFrequency(15.f);
	SetMinNetUpdateFrequency(1.f);
	SetCanBeDamaged(false);
	Items.OwnerPack = this;
	for (int32 Index = 0; Index < MaxItems; ++Index)
	{
		MemberSlot[Index] = INDEX_NONE;
	}
}

void AGSTN_MinionPack::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AGSTN_MinionPack, PackId, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AGSTN_MinionPack, Items, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AGSTN_MinionPack, NetItemSlots, Params);
}

void AGSTN_MinionPack::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// The array may have been copied from the archetype: point it back at this instance
	Items.OwnerPack = this;
}

bool AGSTN_MinionPack::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// Relevancy is only ever asked on the authority; without an active minion subsystem there is nothing to send
	if (const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld()))
	{
		return Minions->IsPackNetRelevantFor(*this, RealViewer, SrcLocation);
	}
	return false;
}

void AGSTN_MinionPack::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A client loses the pack when it becomes irrelevant or is destroyed: its items leave the client view with it
	if (const UWorld* World = GetWorld())
	{
		if (UGSTN_MinionClientRegistry* Registry = World->GetSubsystem<UGSTN_MinionClientRegistry>())
		{
			Registry->OnPackGone(*this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

int32 AGSTN_MinionPack::FindItemIndex(uint8 ItemId) const
{
	for (int32 Index = 0; Index < Items.Items.Num(); ++Index)
	{
		if (Items.Items[Index].Wire.Id == ItemId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AGSTN_MinionPack::InitPack(int32 InPackId, int32 InSpawnGroup)
{
	PackId = InPackId;
	SpawnGroup = InSpawnGroup;
	MARK_PROPERTY_DIRTY_FROM_NAME(AGSTN_MinionPack, PackId, this);
}

void AGSTN_MinionPack::MarkItemsPropertyDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AGSTN_MinionPack, Items, this);
	// Every membership change goes through MutatePack (and so here), after the pack was woken or flushed
	RefreshNetItemSlots();
}

void AGSTN_MinionPack::RefreshNetItemSlots()
{
	bool bChanged = NetItemSlots.Num() != MaxItems;
	NetItemSlots.SetNum(MaxItems);
	for (int32 Id = 0; Id < MaxItems; ++Id)
	{
		const uint16 Value = MemberSlot[Id] == INDEX_NONE ? MAX_uint16 : static_cast<uint16>(MemberSlot[Id]);
		bChanged |= NetItemSlots[Id] != Value;
		NetItemSlots[Id] = Value;
	}
	if (bChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AGSTN_MinionPack, NetItemSlots, this);
	}
}

int32 AGSTN_MinionPack::GetNetSlotOfItem(uint8 ItemId) const
{
	return NetItemSlots.IsValidIndex(ItemId) && NetItemSlots[ItemId] != MAX_uint16 ? static_cast<int32>(NetItemSlots[ItemId]) : INDEX_NONE;
}
