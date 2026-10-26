// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatInbox.h"

#include "Algo/StableSort.h"
#include "Simulation/GSTN_HitInbox.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatInbox, Log, All);

FGSTN_InboxKindRegistry::FGSTN_InboxKindRegistry()
{
	// The hit is the one kind the combat plugin owns. The victim step consumes the hits of a drain as one batch, so it has no handler;
	// the key keeps the order the hit inbox always had inside one attacker: its hitbox, then the hurtbox it picked
	Register(GSTNInboxKinds::Hit, TEXT("Hit"), FHandler(), [](const FGSTN_InboxEntry& Entry) -> uint32
	{
		const FGSTN_HitInboxEntry* Hit = FGSTN_HitInboxEntry::FromInboxEntry(Entry);
		return Hit ? (static_cast<uint32>(Hit->AttackerHitboxIndex) << 8) | Hit->HurtboxIndex : 0u;
	});
}

FGSTN_InboxKindRegistry& FGSTN_InboxKindRegistry::Get()
{
	static FGSTN_InboxKindRegistry Registry;
	return Registry;
}

bool FGSTN_InboxKindRegistry::Register(uint8 KindId, FName Name, FHandler Handler, FOrderKey OrderKey)
{
	if (Find(KindId))
	{
		return false;
	}
	FKind& Kind = Kinds.AddDefaulted_GetRef();
	Kind.Id = KindId;
	Kind.Name = Name;
	Kind.Handler = MoveTemp(Handler);
	Kind.OrderKey = MoveTemp(OrderKey);
	return true;
}

void FGSTN_InboxKindRegistry::Unregister(uint8 KindId)
{
	Kinds.RemoveAll([KindId](const FKind& Kind) { return Kind.Id == KindId; });
}

const FGSTN_InboxKindRegistry::FKind* FGSTN_InboxKindRegistry::Find(uint8 KindId) const
{
	return Kinds.FindByPredicate([KindId](const FKind& Kind) { return Kind.Id == KindId; });
}

uint32 FGSTN_InboxKindRegistry::GetOrderKey(const FGSTN_InboxEntry& Entry) const
{
	const FKind* Kind = Find(Entry.Kind);
	return Kind && Kind->OrderKey ? Kind->OrderKey(Entry) : 0u;
}

bool FGSTN_InboxKindRegistry::Dispatch(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
{
	const FKind* Kind = Find(Entry.Kind);
	if (!Kind)
	{
		bool bAlreadyWarned = false;
		WarnedKinds.Add(Entry.Kind, &bAlreadyWarned);
		if (!bAlreadyWarned)
		{
			UE_LOG(LogGSTNCombatInbox, Warning, TEXT("Combat inbox: no handler is registered for entry kind %d (sent by %d at frame %d): the entry is ignored. Is the plugin that owns the kind loaded on this machine?"),
				Entry.Kind, Entry.SenderId, Entry.Frame);
		}
		return false;
	}
	if (Kind->Handler)
	{
		Kind->Handler(Entry, Context);
	}
	return true;
}

bool FGSTN_CombatInbox::Add(FGSTN_InboxEntry Entry)
{
	if (Entries.Num() >= MaxEntries)
	{
		return false;
	}
	Entries.Add(MoveTemp(Entry));
	return true;
}

void FGSTN_CombatInbox::Drain(int32 Frame, TArray<FGSTN_InboxEntry>& Out)
{
	const int32 First = Out.Num();
	for (int32 Index = 0; Index < Entries.Num();)
	{
		if (Entries[Index].Frame < Frame)
		{
			Out.Add(MoveTemp(Entries[Index]));
			Entries.RemoveAt(Index, EAllowShrinking::No);
		}
		else
		{
			++Index;
		}
	}

	// Stable on the arrival order for entries that tie on all keys, which is the same on every machine (one sender, one frame)
	const FGSTN_InboxKindRegistry& Registry = FGSTN_InboxKindRegistry::Get();
	Algo::StableSort(MakeArrayView(Out.GetData() + First, Out.Num() - First), [&Registry](const FGSTN_InboxEntry& A, const FGSTN_InboxEntry& B)
	{
		if (A.SenderId != B.SenderId)
		{
			return A.SenderId < B.SenderId;
		}
		if (A.Kind != B.Kind)
		{
			return A.Kind < B.Kind;
		}
		return Registry.GetOrderKey(A) < Registry.GetOrderKey(B);
	});
}

void FGSTN_CombatInbox::TrimFrom(int32 FirstResimulatedFrame)
{
	Entries.RemoveAll([FirstResimulatedFrame](const FGSTN_InboxEntry& Entry) { return Entry.Frame >= FirstResimulatedFrame; });
}
