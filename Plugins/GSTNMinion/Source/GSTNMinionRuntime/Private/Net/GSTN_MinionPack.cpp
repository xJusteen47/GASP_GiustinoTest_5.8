// Minion runtime (replication, M8 G6).

#include "Net/GSTN_MinionPack.h"

#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/UnrealNetwork.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "UObject/CoreNet.h"

namespace
{
	/** Types with an extension byte, by type index. Empty while no type has one, which keeps the format at the v1 layout */
	TBitArray<> GExtTypes;
	bool GAnyExtType = false;

	/** Anchor of the pack stream being (de)serialized on this thread; null outside FGSTN_MinionNetArray::NetDeltaSerialize */
	thread_local const FGSTN_MinionNetAnchor* GStreamAnchor = nullptr;

	TAutoConsoleVariable<bool> CVarRelativePositions(
		TEXT("gstn.Minion.Net.RelativePositions"),
		true,
		TEXT("Sender side: minion positions travel relative to a per-pack-stream anchor (lossless, fewer bits). 0 = absolute 19-bit positions."));

	/** Authority, process-wide: what the anchor saved since the last print (game thread only, like the replication that fills it) */
	struct FAnchorStats
	{
		int64 Streams = 0;
		int64 RelativeStreams = 0;
		int64 AbsoluteBits = 0;
		int64 SentBits = 0;
		int64 WidthSum = 0;
	};
	FAnchorStats GAnchorStats;

	FAutoConsoleCommand AnchorStatsCommand(
		TEXT("gstn.Minion.Net.AnchorStats"),
		TEXT("Prints and resets the pack-relative position stats: pack streams sent, how many used the anchor, bits sent vs all-absolute."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const FAnchorStats& S = GAnchorStats;
			UE_LOG(LogTemp, Display, TEXT("MinionAnchor streams=%lld relative=%lld absoluteBits=%lld sentBits=%lld saved=%.1f%% avgWidthSum=%.1f"),
				S.Streams, S.RelativeStreams, S.AbsoluteBits, S.SentBits,
				S.AbsoluteBits > 0 ? 100.0 * (S.AbsoluteBits - S.SentBits) / S.AbsoluteBits : 0.0,
				S.Streams > 0 ? static_cast<double>(S.WidthSum) / S.Streams : 0.0);
			GAnchorStats = FAnchorStats();
		}));

	/** v1 wire format: no global id field (re-pack fallback off); the extension byte only for the types that own one (a bridge's) */
	FGSTN_MinionNetFormat PackItemFormat()
	{
		FGSTN_MinionNetFormat Format;
		Format.ExtTypes = GAnyExtType ? &GExtTypes : nullptr;
		return Format;
	}

	UGSTN_MinionClientRegistry* RegistryOf(const FGSTN_MinionNetArray& Array)
	{
		const UWorld* World = Array.OwnerPack ? Array.OwnerPack->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UGSTN_MinionClientRegistry>() : nullptr;
	}
}

void AGSTN_MinionPack::SetNetExtTypes(const TBitArray<>& InExtTypes)
{
	GExtTypes = InExtTypes;
	GAnyExtType = GExtTypes.Find(true) != INDEX_NONE;
}

bool FGSTN_MinionNetItem::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	FGSTN_MinionNetFormat Format = PackItemFormat();
	Format.Anchor = GStreamAnchor;
	bOutSuccess = FGSTN_MinionNetCodec::Serialize(Ar, Wire, Format);
	return true;
}

bool FGSTN_MinionNetArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	FGSTN_MinionNetAnchor Anchor;
	bool bRelative = false;
	if (DeltaParms.Writer && CVarRelativePositions.GetValueOnAnyThread() && !Items.IsEmpty())
	{
		// Two passes, keep the smaller: the anchor header only pays off when enough items of the pack change in this stream, and only
		// the FastArray knows which ones do. Calling it twice is safe: the write path only rebuilds the same new base state
		TArray<const uint32*, TInlineAllocator<AGSTN_MinionPack::MaxItems>> Positions;
		for (const FGSTN_MinionNetItem& Item : Items)
		{
			Positions.Add(Item.Wire.PositionQ);
		}
		Anchor = FGSTN_MinionNetAnchor::FromPositions(Positions);

		FBitWriter* const OutWriter = DeltaParms.Writer;
		FNetBitWriter RelativeWriter(DeltaParms.Map, 0);
		FNetBitWriter AbsoluteWriter(DeltaParms.Map, 0);
		DeltaParms.Writer = &RelativeWriter;
		bool bWrote = false;
		{
			TGuardValue<const FGSTN_MinionNetAnchor*> AnchorGuard(GStreamAnchor, &Anchor);
			bWrote = FFastArraySerializer::FastArrayDeltaSerialize<FGSTN_MinionNetItem, FGSTN_MinionNetArray>(Items, DeltaParms, *this);
		}
		if (bWrote)
		{
			DeltaParms.Writer = &AbsoluteWriter;
			bWrote = FFastArraySerializer::FastArrayDeltaSerialize<FGSTN_MinionNetItem, FGSTN_MinionNetArray>(Items, DeltaParms, *this);
		}
		DeltaParms.Writer = OutWriter;
		if (!bWrote)
		{
			return false;
		}
		bRelative = RelativeWriter.GetNumBits() + FGSTN_MinionNetAnchor::HeaderBits < AbsoluteWriter.GetNumBits();
		GAnchorStats.Streams++;
		GAnchorStats.RelativeStreams += bRelative ? 1 : 0;
		GAnchorStats.AbsoluteBits += AbsoluteWriter.GetNumBits();
		GAnchorStats.SentBits += 1 + (bRelative ? RelativeWriter.GetNumBits() + FGSTN_MinionNetAnchor::HeaderBits : AbsoluteWriter.GetNumBits());
		GAnchorStats.WidthSum += Anchor.Bits[0] + Anchor.Bits[1] + Anchor.Bits[2];

		OutWriter->WriteBit(bRelative ? 1 : 0);
		if (bRelative)
		{
			FGSTN_MinionNetAnchor::Serialize(*OutWriter, Anchor);
		}
		FNetBitWriter& Chosen = bRelative ? RelativeWriter : AbsoluteWriter;
		OutWriter->SerializeBits(Chosen.GetData(), Chosen.GetNumBits());
		return true;
	}
	if (DeltaParms.Writer)
	{
		DeltaParms.Writer->WriteBit(0);
	}
	else if (DeltaParms.Reader)
	{
		bRelative = DeltaParms.Reader->ReadBit() != 0;
		if (bRelative && !FGSTN_MinionNetAnchor::Serialize(*DeltaParms.Reader, Anchor))
		{
			return false;
		}
		if (DeltaParms.Reader->IsError())
		{
			return false;
		}
	}
	// Items read the anchor through the thread-local while the FastArray (de)serializes this stream, and only then
	TGuardValue<const FGSTN_MinionNetAnchor*> AnchorGuard(GStreamAnchor, bRelative ? &Anchor : nullptr);
	return FFastArraySerializer::FastArrayDeltaSerialize<FGSTN_MinionNetItem, FGSTN_MinionNetArray>(Items, DeltaParms, *this);
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
