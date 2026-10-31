// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_LinkInboxKinds.generated.h"

struct FGSTN_InboxEntry;
struct FGSTN_InboxDispatchContext;
struct FGSTN_LinkFragment;

/** The inbox kinds the link plugin owns, inside its range of GSTNInboxKinds (FirstLink..LastLink) */
namespace GSTNLinkInboxKinds
{
	/** The target's answer to a link request (FGSTN_LinkResponseEntry), to the initiator */
	constexpr uint8 Response = GSTNInboxKinds::FirstLink;
	/** A break or a placement of a live link (FGSTN_LinkCommandEntry), the watchdog's to both sides (F11) */
	constexpr uint8 Command = GSTNInboxKinds::FirstLink + 1;
	static_assert(Command <= GSTNInboxKinds::LastLink, "the link kinds stay in the link range");
}

/**
 * The payload of a LinkResponse entry (m6 spec 7.2 item 2-3): posted by the target's LinkRequest step (an outgoing entry of the hit, stamped
 * with the target's frame F+1), consumed by the initiator at F+2. Plain integers: the type is the registry's wire index (the rank of its tag
 * name, the same on every machine), never a tag.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkResponseEntry
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAccepted = false;

	UPROPERTY()
	uint8 LinkTypeIndex = 0;

	/** The serial of the link the target formed (accepted), or the one it refused */
	UPROPERTY()
	uint8 Serial = 0;

	/** The frame the target formed the link on (its role holds from there) */
	UPROPERTY()
	int32 LinkFrame = INDEX_NONE;

	/** EGSTN_LinkRefusal of a refusal, for logs and tests only */
	UPROPERTY()
	uint8 Refusal = 0;
};

/** What a LinkCommand does */
UENUM()
enum class EGSTN_LinkCommandKind : uint8
{
	None,
	/** The link ends on this side: state cleared (serial kept), the exit recorded */
	Break,
	/** The parked side is moved (an exit placement or a throw's release point) without ending the link */
	Place,
	/**
	 * A link formed by a command instead of a request hit (the S1 spike's server-side test command, debug tools): the channel takes Role of
	 * LinkTypeIndex with PartnerId, the command's serial and the frame after the stamp, when it is free and the serial is newer. Never posted
	 * by the simulation itself
	 */
	Form
};

/**
 * The payload of a LinkCommand entry (m6 spec 7.6): stamped F by the watchdog (F11) and posted to both sides' inboxes, consumed at F+1 on
 * both, so the two sides clear their link on the same frame. Applied only to the live link of the very Serial (FGSTN_LinkState::MatchesBreak),
 * so a stale or a repeated command does nothing.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkCommandEntry
{
	GENERATED_BODY()

	UPROPERTY()
	EGSTN_LinkCommandKind Kind = EGSTN_LinkCommandKind::None;

	UPROPERTY()
	EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;

	UPROPERTY()
	uint8 Serial = 0;

	/** Form only: the type, the role this side takes and its partner's entity id */
	UPROPERTY()
	uint8 LinkTypeIndex = 0;

	UPROPERTY()
	uint8 Role = 0;

	UPROPERTY()
	int32 PartnerId = INDEX_NONE;

	/** The registry's reason index (EGSTN_LinkBreakReason or a plugin's) */
	UPROPERTY()
	uint8 Reason = 0;

	/** Where the side leaves and how fast (quantised floats: the watchdog's placer result) */
	UPROPERTY()
	FVector3f ExitPosition = FVector3f::ZeroVector;

	UPROPERTY()
	float ExitYawDeg = 0.f;

	UPROPERTY()
	FVector3f ExitVelocity = FVector3f::ZeroVector;
};

/**
 * The handlers of the two link kinds (m6 spec 5.2 step 2): they write the receiver's link fragment only, deterministically, from the payload
 * and the frame; the same on every machine and in a re-simulation.
 *
 *  - Response, on the initiator (F+2): accepted and newer than the channel's serial -> role 1, Active, the target as partner, the response's
 *    serial and link frame (the predicted state, if any, is replaced). Refused -> a Pending request to that target is cleared. Idempotent: a
 *    response whose serial the channel already carries as role 1 changes nothing, so the entry a re-simulated sender posts again after a
 *    rollback lands on the same state.
 *  - Command, on both sides (F+1): Break of the live link of that serial -> the channel resets (serial kept) and the exit goes to the link
 *    actions; Place -> the exit only; Form (test and debug command) -> the role of the payload on a free channel. Any other serial is ignored.
 */
struct GSTNLINKNP_API FGSTN_LinkInboxHandlers
{
	static void HandleResponse(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context);
	static void HandleCommand(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context);

	/** The response rule on a fragment (what HandleResponse does), for the tests. SenderId is the target's entity id. True when it changed */
	static bool ApplyResponse(FGSTN_LinkFragment& Fragment, const FGSTN_LinkResponseEntry& Response, int32 SenderId);

	/** The command rule on a fragment. EntryFrame is the command's stamp. True when it changed */
	static bool ApplyCommand(FGSTN_LinkFragment& Fragment, const FGSTN_LinkCommandEntry& Command, int32 EntryFrame);

	/** Registers both kinds with FGSTN_InboxKindRegistry (module startup); false when an id was taken */
	static bool Register();
	static void Unregister();

	/** The entries a step appends to OutgoingEntries (the victim step stamps Frame and SenderId) */
	static FGSTN_InboxEntry MakeResponse(const FGSTN_LinkResponseEntry& Payload);
	static FGSTN_InboxEntry MakeCommand(const FGSTN_LinkCommandEntry& Payload);
};
