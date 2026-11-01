// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_DefenceInboxKinds.generated.h"

struct FGSTN_InboxEntry;
struct FGSTN_InboxDispatchContext;

/** The inbox kinds the defence owns, inside its range of GSTNInboxKinds (FirstDefence..LastDefence) */
namespace GSTNDefenceInboxKinds
{
	/** A parry's punish of the attacker (FGSTN_DefencePunishEntry) */
	constexpr uint8 Punish = GSTNInboxKinds::FirstDefence;
	static_assert(Punish >= GSTNInboxKinds::FirstDefence && Punish <= GSTNInboxKinds::LastDefence, "the defence kinds stay in the defence range");
}

/** What a punish stands for (FGSTN_DefenceFragment::PunishKind) */
namespace GSTNPunishKinds
{
	constexpr uint8 None = 0;
	/** The attacker's hit was parried */
	constexpr uint8 Parried = 1;
}

/**
 * The payload of a Punish entry (m6 spec 5.4, "an outgoing entry Punish{Parried, AttackerStunFrames[HitClass]} to the attacker's inbox"):
 * posted by the victim's simulation after a parry, consumed by the attacker's at the frame after (two frames after the hit). Plain integers,
 * no pointer, no tag: the receiver may run on another machine.
 */
USTRUCT()
struct GSTNDEFENCENP_API FGSTN_DefencePunishEntry
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Kind = GSTNPunishKinds::Parried;

	/** The parried hit's stamped frame: the punish is anchored on it, never on the frame it arrives */
	UPROPERTY()
	int32 HitFrame = INDEX_NONE;

	/** The parry hitstop both sides take from the hit frame */
	UPROPERTY()
	int32 HitstopFrames = 0;

	/** The attacker's stun after that hitstop (the profile's AttackerStunFrames for the hit class) */
	UPROPERTY()
	int32 StunFrames = 0;

	int32 GetHitstopEndFrame() const { return HitFrame + FMath::Max(HitstopFrames, 0); }
};

/**
 * The handler of the Punish kind (m6 spec 5.2 step 2): the attacker becomes Punished until PunishEndFrame (the end of the parry hitstop plus
 * the stun; the action state is derived from the stamp, BlocksActions vetoes every command), its running timeline is cancelled, and its
 * hitstop is extended to the parry's when its own prediction did not open it (a disagreement of the snapshots; the usual case predicted it).
 * Writes the receiver's sync state only, deterministically, from the payload and the frame: the same on every machine and re-simulation.
 */
struct GSTNDEFENCENP_API FGSTN_DefencePunishHandler
{
	static void Handle(const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context);

	/** Registers the kind with FGSTN_InboxKindRegistry (module startup); false when the id was taken */
	static bool Register();
	static void Unregister();

	/** The entry a parry step appends to OutgoingEntries (the victim step stamps Frame and SenderId) */
	static FGSTN_InboxEntry MakeEntry(const FGSTN_DefencePunishEntry& Payload);
};
