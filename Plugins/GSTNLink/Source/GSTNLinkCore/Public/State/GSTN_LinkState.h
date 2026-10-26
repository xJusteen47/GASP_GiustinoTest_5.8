// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameplayTagContainer.h"
#include "Sync/GSTN_EntityRef.h"
#include "GSTN_LinkState.generated.h"

class UGSTN_LinkTypeDefinition;
struct FGSTN_GrabRules;
struct FGSTN_LatchRules;

/** Where a link is in its life (m6 spec 7.1). Written by the link tick and the watchdog's commands, never by an ability */
UENUM(BlueprintType)
enum class EGSTN_LinkPhase : uint8
{
	None,
	/** A predicted link awaiting its response */
	Pending,
	Active,
	/** Break decided; the exit is in flight */
	Exiting
};

/**
 * Why a link broke, in the wire form (m6 spec 7.5): the reason index. The generic reasons below are the registry's first entries, in
 * this order; a plugin appends its own from FirstPlugin on. The tags are the reasons' names (GSTNLinkTags::BreakReason_*), mapped
 * by UGSTN_LinkMath::BreakReasonTag, so nothing on the wire carries a tag.
 */
UENUM(BlueprintType)
enum class EGSTN_LinkBreakReason : uint8
{
	None = 0,
	ShakeOff,
	Timeout,
	Distance,
	OwnDeath,
	PartnerDeath,
	Hit,
	PartnerState,
	Escape,
	Release,
	Throw,
	Forced,
	/** First index a plugin's own reasons (M7: Dismount, Drop, ...) may take */
	FirstPlugin = 16
};

namespace GSTNLinkFlags
{
	/** The initiator's own machine formed this link before the response arrived */
	constexpr uint8 Predicted = 1 << 0;

	/** A break was decided and waits for the watchdog's command (Release, Escape...) */
	constexpr uint8 PendingBreak = 1 << 1;
}

/**
 * The state of one link channel of one entity (m6 spec 7.1): plain data, trivially copyable, indices and ticks only. It holds NO tag
 * (a tag is not stable on the wire): the link type is an index into the registry's table, the role an index into the definition's
 * two roles (0 = none, 1 = initiator, 2 = target), the break reason an index of EGSTN_LinkBreakReason.
 *
 * It is ordinary sync state, restored with the model on a rollback. The inbox entries that form or break a link are not restored
 * (they re-arrive from re-simulated senders), so every entry carries the Serial of the link it is about and the state ignores an
 * entry that is already applied or older (IsFormationNew, MatchesBreak): applying an entry twice does nothing.
 */
struct GSTNLINKCORE_API FGSTN_LinkState
{
	/** Index into the registry's table of link types; 0 = no link */
	uint8 LinkTypeIndex = 0;

	/** 0 = none, 1 = the initiator's role, 2 = the target's role */
	uint8 Role = 0;

	EGSTN_LinkPhase Phase = EGSTN_LinkPhase::None;

	/** The other entity of the link, by neutral reference (a fighter, a minion or an object) */
	FGSTN_EntityRef PartnerId;

	/** Incremented per formation; tells an old link from a new one. 0 = never linked */
	uint8 Serial = 0;

	/** The combat frame the link formed on */
	int32 LinkFrame = 0;

	/** The frame the link ends by itself (the mash end, adjusted by presses; Escape). 0 = none */
	int32 EndFrame = 0;

	/** GSTNLinkFlags bits */
	uint8 Flags = 0;

	/** EGSTN_LinkBreakReason index of the break that is pending or just happened */
	uint8 BreakReason = 0;

	/** Type-specific byte (a carry load...); this plugin never reads it */
	uint8 Payload = 0;

	/** Facing relative to the anchor, quantised to 1/32768 turn */
	int16 LocalYawQ = 0;

	/** Nothing is linked and nothing is pending. The serial is kept apart: it survives the end of a link */
	bool IsIdle() const { return Phase == EGSTN_LinkPhase::None && LinkTypeIndex == 0 && Role == 0; }

	/** The entity holds a role of a link: Combat.Link.Linked is projected from this for the Body channel */
	bool IsLinked() const { return Role != 0 && Phase != EGSTN_LinkPhase::None; }

	bool IsActive() const { return Phase == EGSTN_LinkPhase::Active; }
	bool HasFlag(uint8 Flag) const { return (Flags & Flag) != 0; }
	EGSTN_LinkBreakReason GetBreakReason() const { return static_cast<EGSTN_LinkBreakReason>(BreakReason); }

	/** An entry that forms a link with this serial changes the state only if it is newer than the serial the state already carries */
	bool IsFormationNew(uint8 EntrySerial) const { return IsSerialNewer(EntrySerial, Serial); }

	/** An entry that breaks a link applies only to the live link of that very serial; a break for an older or a newer link is ignored */
	bool MatchesBreak(uint8 EntrySerial) const { return Phase != EGSTN_LinkPhase::None && EntrySerial == Serial; }

	/** Clears the link and keeps the serial, so a late entry of the old link stays old */
	void Reset()
	{
		const uint8 KeptSerial = Serial;
		*this = FGSTN_LinkState();
		Serial = KeptSerial;
	}

	bool operator==(const FGSTN_LinkState& Other) const;
	bool operator!=(const FGSTN_LinkState& Other) const { return !(*this == Other); }

	/**
	 * Writes or reads the state. An idle state is a single zero byte on the wire (it costs about one bit in the fragment's bit
	 * stream); a live one is the flag byte and the fields, every one of them fixed size. Equal states have equal bytes
	 */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	FString ToString() const;

	/** Whether Candidate is later than Current on the wrapping 8-bit serial circle (half the circle counts as later) */
	static bool IsSerialNewer(uint8 Candidate, uint8 Current) { return Candidate != Current && static_cast<uint8>(Candidate - Current) < 128; }

	/** The serial of a new link between two entities: one after the newer of the two serials, never 0 */
	static uint8 NextSerial(uint8 A, uint8 B);
};

/** The two link slots of an entity (m6 spec 7.0 item 2): the Body slot parks, the Hand slot never does */
struct GSTNLINKCORE_API FGSTN_LinkStates
{
	FGSTN_LinkState Body;
	FGSTN_LinkState Hand;

	FGSTN_LinkState& Get(EGSTN_LinkChannel Channel) { return Channel == EGSTN_LinkChannel::Body ? Body : Hand; }
	const FGSTN_LinkState& Get(EGSTN_LinkChannel Channel) const { return Channel == EGSTN_LinkChannel::Body ? Body : Hand; }

	bool operator==(const FGSTN_LinkStates& Other) const { return Body == Other.Body && Hand == Other.Hand; }
};

/**
 * The generic block of link actions and partner moves (m6 spec 7.0 item 5): what a link type's action provider plays on each side,
 * and the exit of a break. Plain data like the link state; a type that has no partner moves leaves it idle.
 */
struct GSTNLINKCORE_API FGSTN_LinkActions
{
	/** The serial of the last link request this entity refused, so a re-arriving request is not answered twice */
	uint16 LastRefusedSerial = 0;

	/** The frame of that refusal */
	int32 RefusedFrame = 0;

	/** The frame before which this entity cannot form a link again (a remount cooldown, the grabber's cooldown after a throw) */
	int32 CooldownEndFrame = 0;

	/** The partner move that is playing: the action provider's id and the frame it started */
	struct FActive
	{
		int16 Id = 0;
		int32 StartFrame = 0;
	} Active;

	/** A partner move that starts at Frame */
	struct FPendingStart
	{
		int32 Frame = 0;
		int16 Id = 0;
	} PendingStart;

	/** The exit the watchdog decided: where and how fast the entity leaves, valid when Frame is non-zero */
	struct FExit
	{
		int32 Frame = 0;
		FVector Position = FVector::ZeroVector;
		float YawDeg = 0.f;
		FVector Velocity = FVector::ZeroVector;
		uint8 Flags = 0;
	} Exit;

	bool IsIdle() const { return *this == FGSTN_LinkActions(); }
	bool operator==(const FGSTN_LinkActions& Other) const;
};

/** What a request rule refuses a link for (m6 spec 7.0 item 4). The reason is for logs, tests and UI; it never reaches the wire */
UENUM(BlueprintType)
enum class EGSTN_LinkRefusal : uint8
{
	None,
	/** The context carries no definition */
	NoDefinition,
	/** Initiator and target are one entity */
	SelfLink,
	/** The initiator already holds a link on this channel */
	InitiatorBusy,
	/** The target already holds a link on this channel (a second grabber, a second latcher) */
	TargetBusy,
	/** A link of the other channel forbids this one (ForbiddenWithBody) */
	CoexistForbidden,
	/** The target carries Combat.Lock.Link */
	TargetLocked,
	/** The definition's RequestRequirements are not met by the target */
	RequestRequirements,
	/** The rule's tag requirements are not met by the target */
	Requirements,
	/** The target carries Combat.Part.GrabLocked (read by name) */
	GrabLocked,
	/** The target is heavier than the grab limit */
	TooHeavy,
	/** The target is lighter than the latch limit */
	TooLight,
	/** The target is still immune after its last link */
	Immunity
};

/** The answer of a request rule: Accept or Refuse(reason). A default value accepts, so a rule that does nothing is the identity */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkRuleResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	bool bAccepted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	EGSTN_LinkRefusal Refusal = EGSTN_LinkRefusal::None;

	static FGSTN_LinkRuleResult Accept() { return FGSTN_LinkRuleResult(); }

	static FGSTN_LinkRuleResult Refuse(EGSTN_LinkRefusal Reason)
	{
		FGSTN_LinkRuleResult Result;
		Result.bAccepted = false;
		Result.Refusal = Reason;
		return Result;
	}
};

/** One entity of a link request, as the target side's resolution sees it at the end of the previous frame: values, no actor, no component */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkRequestSide
{
	GENERATED_BODY()

	/** The pipeline's entity id (the Network Prediction proxy id for a fighter); INDEX_NONE for an entity without one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	int32 EntityId = INDEX_NONE;

	/** The state tags the entity projects (Combat.State.*, Combat.Link.*, immunities, locks) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	FGameplayTagContainer Tags;

	/** The weight a grab or latch is decided by: the health-based weight curve, or the parts plugin's effective weight with lost limbs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link", meta = (ClampMin = "0.0"))
	float EffectiveWeight = 100.f;

	/** The entity cannot be grabbed before this frame (set when its last link ended) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	int32 GrabImmunityEndFrame = 0;

	/** The entity cannot be latched onto before this frame (set when its last latcher left) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	int32 LatchImmunityEndFrame = 0;

	/** Link states of the two channels (plain data, not reflected) */
	FGSTN_LinkStates Links;

	/** The definition of the link on the channel other than the request's, or null when that channel is free (not reflected) */
	const UGSTN_LinkTypeDefinition* OtherChannelDefinition = nullptr;
};

/**
 * Everything a request rule may read (m6 spec 7.0 item 4): a const view of both entities and the link type, built by the pipeline's
 * LinkRequest step from the snapshot of the previous frame. A rule reads nothing else, so it is deterministic for equal inputs on
 * every machine and a Blueprint rule cannot reach into a world.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_LinkRequestContext
{
	GENERATED_BODY()

	/** The combat frame the request is decided on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	int32 Frame = 0;

	/** The link type the request asks for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link", meta = (Categories = "Combat.Link.Type"))
	FGameplayTag LinkType;

	/** The entity whose hit or command forms the link (role 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	FGSTN_LinkRequestSide Initiator;

	/** The entity the link is formed with (role 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Link")
	FGSTN_LinkRequestSide Target;

	/** The definition of LinkType. Not reflected: valid for the call, a rule never keeps it */
	const UGSTN_LinkTypeDefinition* Definition = nullptr;

	/** The initiator's kit grab rules, or null for the rule's own copy. Not reflected */
	const FGSTN_GrabRules* InitiatorGrabRules = nullptr;

	/** The initiator's kit latch rules, or null for the rule's own copy. Not reflected */
	const FGSTN_LatchRules* InitiatorLatchRules = nullptr;
};
