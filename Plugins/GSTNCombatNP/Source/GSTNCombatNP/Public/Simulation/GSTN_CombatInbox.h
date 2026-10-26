// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Sync/GSTN_SyncFragment.h"

struct FGSTN_CombatSyncState;
class UGSTN_CombatSimComponent;

/**
 * One message on its way to a combat entity (combat-m4-hits 7, extended by M6 F1 to open kinds): written by the sender's combat tick
 * at Frame, consumed by the receiver's combat tick at Frame + 1, so the effect is always one frame after the cause and two entities
 * that message each other on the same frame both receive, whatever their NP ids. A hit is kind GSTNInboxKinds::Hit and wraps
 * FGSTN_HitInboxEntry unchanged; the kinds of the defence and link plugins carry their own payload struct.
 *
 * Plain local data, not part of the sync state: the sender's re-simulation writes it again after a rollback, so a rollback only
 * trims the entries of the frames it re-simulates.
 */
struct FGSTN_InboxEntry
{
	/** A GSTNInboxKinds id: which handler consumes the payload and which payload struct it holds */
	uint8 Kind = GSTNInboxKinds::Hit;

	/** The simulation frame the sender's tick wrote the entry in: the delivery gate (the receiver drains entries stamped before its own frame) */
	int32 Frame = INDEX_NONE;

	/** The NP proxy id of the sender: the first key of the drain order */
	int32 SenderId = INDEX_NONE;

	/** The kind's data. Plain data of the kind's own struct: no pointer into a machine's objects, the receiver may run on another one */
	FInstancedStruct Payload;
};

/** What a kind's handler may read and change when the receiver drains its entries */
struct FGSTN_InboxDispatchContext
{
	/** The frame the receiver's tick is on */
	int32 Frame = INDEX_NONE;

	/** The receiver's combat sync state of this frame: the one place a handler writes simulated state (never a component field) */
	FGSTN_CombatSyncState* Sync = nullptr;

	/** The receiver. Null in a test that drains without a component */
	UGSTN_CombatSimComponent* Receiver = nullptr;
};

/**
 * The kinds the combat inboxes understand: kind id to handler, registered by the NP modules that add a kind (like
 * FGSTN_OutcomeApplierRegistry for outcome extensions). The hit kind is registered by this class itself.
 *
 * Handlers run when the receiver drains, in the drain order, before the receiver resolves the frame's hits (the hits are consumed by
 * the victim step as one batch, so the hit kind has no handler). A handler must be deterministic and write only through its
 * context. An entry of a kind nobody registered is ignored with one warning per kind, so a machine that lacks a plugin still
 * simulates the rest and the missing effect shows as a correction instead of a crash.
 *
 * Registration happens in module startup, before any simulation, and the registry is only read while simulating.
 */
class GSTNCOMBATNP_API FGSTN_InboxKindRegistry
{
public:
	using FHandler = TFunction<void(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)>;

	/** The tie-break inside one (sender, kind): the entries with the lower key come first, equal keys keep their arrival order */
	using FOrderKey = TFunction<uint32(const FGSTN_InboxEntry& Entry)>;

	struct FKind
	{
		uint8 Id = 0;
		FName Name;
		FHandler Handler;
		FOrderKey OrderKey;
	};

	static FGSTN_InboxKindRegistry& Get();

	/**
	 * Adds a kind. A null Handler means the owner consumes the entries itself (the hit kind). @return false when the id is already
	 * taken (the first registration stays) or is not inside a range of GSTNInboxKinds
	 */
	bool Register(uint8 KindId, FName Name, FHandler Handler, FOrderKey OrderKey = FOrderKey());

	void Unregister(uint8 KindId);

	const FKind* Find(uint8 KindId) const;

	/** The order key of Entry's kind, 0 for a kind without one or an unknown kind */
	uint32 GetOrderKey(const FGSTN_InboxEntry& Entry) const;

	/**
	 * Hands Entry to its kind's handler. A kind without a handler (the hit) returns true and does nothing. An unknown kind logs one
	 * warning per kind and returns false. @return false when the entry was ignored
	 */
	bool Dispatch(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context);

	/** Test support: forgets which unknown kinds were already warned about */
	void ResetWarnings() { WarnedKinds.Reset(); }

private:
	FGSTN_InboxKindRegistry();

	TArray<FKind> Kinds;
	TSet<uint8> WarnedKinds;
};

/**
 * The inbox of one combat entity (an inbox kind set of the combat inboxes: the typed entry is the "message", the drain order the
 * deterministic processing order of modularity-and-tags 5b). The entity's component owns it. The container knows nothing about
 * the machine: whether an entry is written at all is decided by UGSTN_CombatSimComponent::PostToInbox, which refuses where the
 * receiving instance does not tick.
 */
class GSTNCOMBATNP_API FGSTN_CombatInbox
{
public:
	/** Entries kept at most, of every kind together. A frame holds one per attacker hitbox that overlaps; more than this is a content error and the extra is dropped */
	static constexpr int32 MaxEntries = 64;

	/** @return false when the inbox is full and the entry was dropped */
	bool Add(FGSTN_InboxEntry Entry);

	/**
	 * Moves the entries stamped before Frame (the frame the receiver's tick is on) into Out, ordered by (SenderId, Kind, the kind's
	 * order key, arrival), and removes them. The stamp of an entry only gates the delivery: a receiver that missed a frame gets the
	 * older entries together, ordered by sender and not by frame, as it always has. Out is appended to, so a caller drains into
	 * scratch it clears itself
	 */
	void Drain(int32 Frame, TArray<FGSTN_InboxEntry>& Out);

	/**
	 * Rollback: drops the entries of every kind stamped at or after FirstResimulatedFrame, the first frame the re-simulation runs. The
	 * senders' re-simulation writes them again; the earlier ones were already delivered to a state that was kept.
	 */
	void TrimFrom(int32 FirstResimulatedFrame);

	int32 Num() const { return Entries.Num(); }
	bool IsEmpty() const { return Entries.IsEmpty(); }
	void Reset() { Entries.Reset(); }
	const TArray<FGSTN_InboxEntry>& GetEntries() const { return Entries; }

private:
	TArray<FGSTN_InboxEntry> Entries;
};
