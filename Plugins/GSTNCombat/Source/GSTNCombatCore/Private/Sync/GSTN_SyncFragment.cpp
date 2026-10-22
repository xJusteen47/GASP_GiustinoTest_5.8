// Pure combat logic (no GAS, no Network Prediction).

#include "Sync/GSTN_SyncFragment.h"

#include "Algo/Sort.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNSyncFragment, Log, All);

namespace
{
	struct FRegistryData
	{
		const UScriptStruct* ById[256] = {};
		TMap<const UScriptStruct*, uint8> IdByStruct;
	};

	FRegistryData& GetRegistryData()
	{
		static FRegistryData Data;
		return Data;
	}

	uint8 IdOf(const TInstancedStruct<FGSTN_SyncFragment>& Fragment)
	{
		return FGSTN_SyncFragmentRegistry::FindId(Fragment.GetScriptStruct());
	}

	/** The fragment of Array with the same struct as Reference, or null */
	const TInstancedStruct<FGSTN_SyncFragment>* FindSameType(const FGSTN_SyncFragmentArray& Array, const UScriptStruct* Struct)
	{
		return Array.FindByPredicate([Struct](const TInstancedStruct<FGSTN_SyncFragment>& Candidate) { return Candidate.GetScriptStruct() == Struct; });
	}

	struct FWriteEntry
	{
		uint8 Id = GSTNFeatureIds::Invalid;
		const TInstancedStruct<FGSTN_SyncFragment>* Source = nullptr;
		TArray<uint8> Payload;
	};

	/**
	 * The fragments of Fragments that NetSerialize would send, serialized: registered, the first of their type id, within the
	 * count and size limits. A fragment that cannot be sent is not part of the replicated state, so ShouldReconcile must not
	 * look at it either (the receiver never has it, which would ask for a reconcile every frame). bReport logs the dropped ones.
	 */
	void CollectSendable(const FGSTN_SyncFragmentArray& Fragments, TArray<FWriteEntry, TInlineAllocator<4>>& Entries, bool bReport)
	{
		for (const TInstancedStruct<FGSTN_SyncFragment>& Fragment : Fragments)
		{
			const uint8 Id = IdOf(Fragment);
			if (Id == GSTNFeatureIds::Invalid)
			{
				ensureMsgf(!bReport, TEXT("A sync fragment of type %s is not registered and is not sent"), Fragment.IsValid() ? *Fragment.GetScriptStruct()->GetName() : TEXT("(empty)"));
				continue;
			}

			bool bDuplicate = false;
			for (const FWriteEntry& Existing : Entries)
			{
				bDuplicate |= Existing.Id == Id;
			}
			if (bDuplicate)
			{
				ensureMsgf(!bReport, TEXT("Two sync fragments with type id %d, the second is not sent"), Id);
				continue;
			}
			if (Entries.Num() >= FGSTN_SyncFragmentList::MaxFragments)
			{
				ensureMsgf(!bReport, TEXT("More than %d sync fragments, the rest is not sent"), FGSTN_SyncFragmentList::MaxFragments);
				continue;
			}

			FWriteEntry Entry;
			Entry.Id = Id;
			Entry.Source = &Fragment;
			FMemoryWriter Writer(Entry.Payload);
			const_cast<FGSTN_SyncFragment&>(Fragment.Get()).NetSerialize(Writer);
			if (Entry.Payload.Num() > FGSTN_SyncFragmentList::MaxFragmentBytes)
			{
				ensureMsgf(!bReport, TEXT("Sync fragment %d is %d bytes, over the limit of %d, and is not sent"), Id, Entry.Payload.Num(), FGSTN_SyncFragmentList::MaxFragmentBytes);
				continue;
			}
			Entries.Add(MoveTemp(Entry));
		}
	}
}

bool FGSTN_SyncFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	// NetSerialize is one function for both directions, hence not const; writing does not modify the fragment
	TArray<uint8> LocalBytes;
	TArray<uint8> AuthorityBytes;
	FMemoryWriter LocalWriter(LocalBytes);
	FMemoryWriter AuthorityWriter(AuthorityBytes);
	const_cast<FGSTN_SyncFragment*>(this)->NetSerialize(LocalWriter);
	const_cast<FGSTN_SyncFragment&>(Authority).NetSerialize(AuthorityWriter);
	return LocalBytes != AuthorityBytes;
}

bool FGSTN_SyncFragmentRegistry::Register(uint8 TypeId, const UScriptStruct* Struct)
{
	if (!ensureMsgf(TypeId != GSTNFeatureIds::Invalid && Struct && Struct->IsChildOf(FGSTN_SyncFragment::StaticStruct()),
		TEXT("A sync fragment needs a non-zero type id and a struct derived from FGSTN_SyncFragment")))
	{
		return false;
	}

	FRegistryData& Data = GetRegistryData();
	if (Data.ById[TypeId] == Struct)
	{
		return true;
	}
	if (!ensureMsgf(!Data.ById[TypeId] && !Data.IdByStruct.Contains(Struct), TEXT("Sync fragment type id %d or struct %s is already registered"), TypeId, *Struct->GetName()))
	{
		return false;
	}

	Data.ById[TypeId] = Struct;
	Data.IdByStruct.Add(Struct, TypeId);
	return true;
}

void FGSTN_SyncFragmentRegistry::Unregister(uint8 TypeId)
{
	FRegistryData& Data = GetRegistryData();
	if (const UScriptStruct* Struct = Data.ById[TypeId])
	{
		Data.IdByStruct.Remove(Struct);
		Data.ById[TypeId] = nullptr;
	}
}

const UScriptStruct* FGSTN_SyncFragmentRegistry::FindStruct(uint8 TypeId)
{
	return GetRegistryData().ById[TypeId];
}

uint8 FGSTN_SyncFragmentRegistry::FindId(const UScriptStruct* Struct)
{
	const uint8* Id = GetRegistryData().IdByStruct.Find(Struct);
	return Id ? *Id : GSTNFeatureIds::Invalid;
}

int32 FGSTN_SyncFragmentList::FindInsertIndex(const FGSTN_SyncFragmentArray& Fragments, uint8 TypeId)
{
	int32 Index = 0;
	while (Index < Fragments.Num() && IdOf(Fragments[Index]) <= TypeId)
	{
		++Index;
	}
	return Index;
}

void FGSTN_SyncFragmentList::NetSerialize(FArchive& Ar, FGSTN_SyncFragmentArray& Fragments)
{
	if (Ar.IsSaving())
	{
		// Everything is serialized before anything is written, because the count goes first and a fragment that cannot be sent
		// (unregistered, a duplicate, too big) must not be in it
		TArray<FWriteEntry, TInlineAllocator<4>> Entries;
		CollectSendable(Fragments, Entries, true);
		Algo::SortBy(Entries, &FWriteEntry::Id);

		uint8 bHasFragments = Entries.IsEmpty() ? 0 : 1;
		Ar.SerializeBits(&bHasFragments, 1);
		if (!bHasFragments)
		{
			return;
		}

		uint32 Count = static_cast<uint32>(Entries.Num());
		Ar.SerializeIntPacked(Count);
		for (FWriteEntry& Entry : Entries)
		{
			Ar << Entry.Id;
			uint32 Length = static_cast<uint32>(Entry.Payload.Num());
			Ar.SerializeIntPacked(Length);
			if (Length > 0)
			{
				Ar.Serialize(Entry.Payload.GetData(), Length);
			}
		}
		return;
	}

	Fragments.Reset();

	uint8 bHasFragments = 0;
	Ar.SerializeBits(&bHasFragments, 1);
	if (!bHasFragments)
	{
		return;
	}

	uint32 Count = 0;
	Ar.SerializeIntPacked(Count);
	if (Count == 0 || Count > static_cast<uint32>(MaxFragments))
	{
		Ar.SetError();
		return;
	}

	TArray<uint8> Payload;
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		uint8 Id = 0;
		Ar << Id;
		uint32 Length = 0;
		Ar.SerializeIntPacked(Length);
		if (Length > static_cast<uint32>(MaxFragmentBytes))
		{
			Ar.SetError();
			return;
		}

		// The payload is always consumed from the archive first, so a fragment that is unknown here or fails to read leaves the
		// reader exactly where the writer's next field is
		Payload.SetNumUninitialized(Length);
		if (Length > 0)
		{
			Ar.Serialize(Payload.GetData(), Length);
		}
		if (Ar.IsError())
		{
			return;
		}

		const UScriptStruct* Struct = FGSTN_SyncFragmentRegistry::FindStruct(Id);
		if (!Struct)
		{
			UE_LOG(LogGSTNSyncFragment, Verbose, TEXT("Sync fragment type id %d is not registered on this machine: skipped (%d bytes)"), Id, Length);
			continue;
		}

		const int32 At = FindInsertIndex(Fragments, Id);
		if (At > 0 && IdOf(Fragments[At - 1]) == Id)
		{
			continue;
		}

		TInstancedStruct<FGSTN_SyncFragment> Fragment;
		Fragment.InitializeAsScriptStruct(Struct);
		FMemoryReader Reader(Payload);
		Fragment.GetMutable().NetSerialize(Reader);
		if (Reader.IsError())
		{
			UE_LOG(LogGSTNSyncFragment, Warning, TEXT("Sync fragment %s (type id %d) could not be read: dropped"), *Struct->GetName(), Id);
			continue;
		}
		Fragments.Insert(MoveTemp(Fragment), At);
	}
}

bool FGSTN_SyncFragmentList::ShouldReconcile(const FGSTN_SyncFragmentArray& Local, const FGSTN_SyncFragmentArray& Authority)
{
	// Only what NetSerialize sends takes part: the authority's copy never holds a fragment that could not be sent, so comparing
	// the raw arrays would ask for a reconcile on every frame
	TArray<FWriteEntry, TInlineAllocator<4>> LocalEntries;
	TArray<FWriteEntry, TInlineAllocator<4>> AuthorityEntries;
	CollectSendable(Local, LocalEntries, false);
	CollectSendable(Authority, AuthorityEntries, false);
	if (LocalEntries.Num() != AuthorityEntries.Num())
	{
		return true;
	}

	for (const FWriteEntry& Entry : LocalEntries)
	{
		const FWriteEntry* Other = AuthorityEntries.FindByPredicate([&Entry](const FWriteEntry& Candidate) { return Candidate.Id == Entry.Id; });
		if (!Other || Entry.Source->Get().ShouldReconcile(Other->Source->Get()))
		{
			return true;
		}
	}
	return false;
}

void FGSTN_SyncFragmentList::Interpolate(FGSTN_SyncFragmentArray& Out, const FGSTN_SyncFragmentArray* From, const FGSTN_SyncFragmentArray& To, float PCT)
{
	Out = To;
	for (int32 Index = 0; Index < Out.Num(); ++Index)
	{
		const TInstancedStruct<FGSTN_SyncFragment>* FromFragment = From ? FindSameType(*From, To[Index].GetScriptStruct()) : nullptr;
		Out[Index].GetMutable().Interpolate(FromFragment ? &FromFragment->Get() : nullptr, &To[Index].Get(), PCT);
	}
}

void FGSTN_SyncFragmentList::ToString(const FGSTN_SyncFragmentArray& Fragments, FAnsiStringBuilderBase& Out)
{
	if (Fragments.IsEmpty())
	{
		return;
	}

	Out.Append(" Fragments:");
	for (const TInstancedStruct<FGSTN_SyncFragment>& Fragment : Fragments)
	{
		Out.Appendf(" [%d: ", IdOf(Fragment));
		Fragment.Get().ToString(Out);
		Out.Append("]");
	}
	Out.Append("\n");
}
