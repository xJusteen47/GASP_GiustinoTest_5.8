// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "Sync/GSTN_SyncFragment.h"

/**
 * One message on its way to a combat entity (combat-m4-hits 7, extended by M6 F1 to open kinds): written by the sender's combat tick
 * at Frame, consumed by the receiver's combat tick at Frame + 1, so the effect is always one frame after the cause and two entities
 * that message each other on the same frame both receive, whatever their NP ids. A hit is kind GSTNInboxKinds::Hit and wraps
 * FGSTN_HitInboxEntry unchanged; the kinds of the defence and link plugins carry their own payload struct.
 *
 * Lives in the core module (header only, no behaviour) so a hit resolution step can name the entries it sends (FGSTN_HitResolutionContext::OutgoingEntries); the
 * inboxes, the kind registry and the delivery stay in the NP module.
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
