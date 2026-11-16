// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkOutcomeTrack.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Simulation/GSTN_HitInbox.h"
#include "GSTN_LinkThrow.generated.h"

class UGSTN_CombatRules;
class UGSTN_LinkRegistry;
class UGSTN_LinkTypeDefinition;
struct FGSTN_CombatSyncCore;
struct FGSTN_CombatTimelineData;
struct FGSTN_GrabRules;
struct FGSTN_LinkFragment;
struct FGSTN_LinkKitExtension;
struct FGSTN_SnapshotEntry;

/**
 * Carried in the ExtensionData of a hit the link plugin posts DIRECTLY to the partner's inbox (a throw: no hitbox, no overlap). The LinkRequest
 * step validates such a hit on the victim's side alone: its Body link is Active, of this serial and type, with the attacker as partner. The
 * attacker's own side is already gone on that frame (it cleared its link at the key), so it is not asked.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkPartnerHit : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Serial = 0;

	UPROPERTY()
	uint8 LinkTypeIndex = 0;
};

/**
 * What the LinkRequest step decided for a throw hit on the thrown entity (m6 spec 8, Throw), applied by FGSTN_LinkThrowOutcomeApplier at the
 * victim's frame F+1: its Body link of this serial ends with reason Throw, its grab immunity end is raised (the thrower's AfterThrow), and its
 * exit is recorded with the Teleport flag at the thrower's socket (where its parked placement already was: the snapshot position the step read),
 * so the exit request moves the capsule there on that frame and the throw's own knockback launches it when its hitstop ends.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkThrowOutcome : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Serial = 0;

	/** The frame the exit applies on: the hit's stamped frame plus one */
	UPROPERTY()
	int32 ExitFrame = 0;

	UPROPERTY()
	int32 ImmunityEndFrame = 0;

	/** Quantised floats, as a LinkCommand carries its exit: every machine applies the same numbers */
	UPROPERTY()
	FVector3f ExitPosition = FVector3f::ZeroVector;

	UPROPERTY()
	float ExitYawDeg = 0.f;
};

/** Applies FGSTN_LinkThrowOutcome to the thrown entity's link fragment, and raises Combat.Event.Throw */
class GSTNLINKNP_API FGSTN_LinkThrowOutcomeApplier : public IGSTN_OutcomeExtensionApplier
{
public:
	virtual const UScriptStruct* GetExtensionStruct() const override;
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override;
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const override;
};

/**
 * The link outcome track of the running move (FGSTN_LinkOutcomeTrack, m6 spec 3 and 8): which of its keys the frame crossed. A key fires once,
 * on the frame the timeline's evaluated frame reaches it (hitstop frames stand still and fire nothing), the same on every machine and in a
 * re-simulation: the frames are read from the start and end states of the frame, never from a clock of its own.
 */
struct GSTNLINKNP_API FGSTN_LinkOutcomeKeys
{
	/**
	 * The timeline frames this tick evaluated past: (OutPrevious, OutCurrent]. False when no timeline runs at the end of the frame. A timeline
	 * that started this frame or the previous one (its frame 0 was evaluated after the hooks) counts from -1, so a key on frame 0 fires
	 */
	static bool GetCrossedFrames(const FGSTN_CombatSyncCore& In, const FGSTN_CombatSyncCore& Out, int32 Frame, int32& OutPrevious, int32& OutCurrent);

	/** Every key of every link outcome track of Data whose timeline frame (track start + key frame) is in (Previous, Current], in track then key order */
	static void Collect(const FGSTN_CombatTimelineData& Data, int32 Previous, int32 Current, TArray<const FGSTN_LinkOutcomeKey*, TInlineAllocator<2>>& OutKeys);
};

/**
 * The grabber's side of a throw (m6 spec 8): the key's hit goes to the partner as a direct inbox entry (no overlap), and the grabber's own Body
 * link ends on the key's frame with reason Throw (serial kept), its grab cooldown set from its grab rules. Pure functions of their inputs; the
 * link tick extension posts the entry through the combat world (as the attacker step posts its hits).
 */
struct GSTNLINKNP_API FGSTN_LinkThrow
{
	/**
	 * The throw's inbox entry for the partner, stamped Frame: the key's ThrowHit (its hit type set to Combat.HitType.Throw when it names none,
	 * its ExtensionData passed through untouched: the reserved slot of the held-objects plugin's thrown body extension) plus FGSTN_LinkPartnerHit
	 * with the link's serial and type. Self and Partner are the two entities' snapshot entries (end of the previous frame). Rules resolves the
	 * relationship the way the attacker step does; null = Enemy
	 */
	static FGSTN_HitInboxEntry MakeThrowEntry(const FGSTN_LinkOutcomeKey& Key, int32 Frame, const FGSTN_SnapshotEntry& Self, const FGSTN_SnapshotEntry& Partner,
		uint8 Serial, uint8 LinkTypeIndex, const UGSTN_CombatRules* Rules);

	/**
	 * Ends the grabber's Body link at Frame for a throw: reset (serial kept), BreakReason Throw, CooldownEndFrame = Frame + the rules'
	 * GrabberCooldownFrames, and an exit record without flags dated Frame + 2 so the idle hygiene drops the reason after it (the watchdog reads the
	 * reason meanwhile and leaves the thrown side to the simulation). False when the Body link is not live
	 */
	static bool EndOwnLink(FGSTN_LinkFragment& Fragment, int32 Frame, const FGSTN_GrabRules* Rules);

	/** The grab rules of a link: the initiator kit's when it carries a link extension, else the type's own grab rule, else null */
	static const FGSTN_GrabRules* FindGrabRules(const UGSTN_LinkTypeDefinition* Definition, const FGSTN_LinkKitExtension* InitiatorKit);

	/** Logs once per run that a thrown body extension (a struct of a disabled plugin) is in the key's data and is ignored */
	static void WarnUnresolvedExtensions(const FGSTN_HitData& ThrowHit);
};
