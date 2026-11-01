// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Hits/GSTN_HitExtensions.h"
#include "GSTN_LinkHitOutcome.generated.h"

struct FGSTN_HitContextView;
struct FGSTN_LinkRequestContext;
struct FGSTN_LinkSnapshotExt;
struct FGSTN_StepResult;

/**
 * What the LinkRequest step decided about a link request hit, handed to the link outcome applier (FGSTN_LinkOutcomeApplier) when the target's
 * victim step applies the hit (its frame F+1). The step is pure and runs on the initiator's prediction too; this struct is the only way its
 * decision reaches the target's link fragment. Integers only, so a re-simulated hit hands over the same bits.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkHitOutcome : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAccepted = false;

	UPROPERTY()
	uint8 LinkTypeIndex = 0;

	UPROPERTY()
	EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;

	/** The new link's serial: one after the newer of the two sides' serials */
	UPROPERTY()
	uint8 Serial = 0;

	/** The frame the target's role holds from: the hit's stamped frame plus one */
	UPROPERTY()
	int32 LinkFrame = INDEX_NONE;

	/** The initiator's entity id (the partner the target records) */
	UPROPERTY()
	int32 InitiatorId = INDEX_NONE;

	/** EGSTN_LinkRefusal of a refusal (logs, tests) */
	UPROPERTY()
	uint8 Refusal = 0;

	/** The frame the target's role ends by itself (the mash end, F12): LinkFrame + the grab rules' initial mash frames; 0 = no mash */
	UPROPERTY()
	int32 EndFrame = 0;
};

/**
 * What the LinkBreakOnHit step decided: the victim's live link of this serial is to break (reason Hit). Handed to FGSTN_LinkBreakOutcomeApplier,
 * which sets PendingBreak on that very link only (a link of another serial is left alone), for the watchdog to break.
 */
USTRUCT()
struct GSTNLINKNP_API FGSTN_LinkBreakOutcome : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	EGSTN_LinkChannel Channel = EGSTN_LinkChannel::Body;

	UPROPERTY()
	uint8 Serial = 0;

	/** EGSTN_LinkBreakReason index (Hit) */
	UPROPERTY()
	uint8 Reason = 0;
};

/** Applies FGSTN_LinkBreakOutcome: PendingBreak with its reason on the victim's live link of the outcome's serial */
class GSTNLINKNP_API FGSTN_LinkBreakOutcomeApplier : public IGSTN_OutcomeExtensionApplier
{
public:
	virtual const UScriptStruct* GetExtensionStruct() const override;
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override;
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const override {}
};

/**
 * Applies the link outcomes to the target's link fragment (m6 spec 7.2 item 2): accepted -> the channel holds role 2 of the type, Active, the
 * initiator as partner, the outcome's serial and link frame; refused -> the refusal is recorded in the link actions (LastRefusedSerial,
 * RefusedFrame). Nothing else: the target's timeline is cancelled by the link tick extension on the frame its role starts blocking actions,
 * and the parked mode follows the same edge (FGSTN_LinkedModifier). AppendEvents raises Combat.Event.LinkFormed for an accepted request, on
 * every machine and re-simulation; the authority publishes it once.
 */
class GSTNLINKNP_API FGSTN_LinkOutcomeApplier : public IGSTN_OutcomeExtensionApplier
{
public:
	virtual const UScriptStruct* GetExtensionStruct() const override;
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override;
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const override;
};

/** Helpers both link steps use */
struct GSTNLINKNP_API FGSTN_LinkStepUtils
{
	/** The hit type is (or is under) Combat.HitType.Grab */
	static bool IsGrabHit(const FGameplayTag& HitType);

	/** The hit type is the link plugin's Throw or Pummel (a hit on the linked partner) */
	static bool IsPartnerHit(const FGameplayTag& HitType);

	/**
	 * The request context of a hit (m6 spec 7.2): the target from the victim view and its link extension, the initiator from the hit's attacker
	 * extension, the definition by the hit's LinkType, the initiator kit's rules. The initiator's predicted request for THIS target (its own
	 * Pending state, in the snapshot of the end of the previous frame) is not counted as busy: it is this very request
	 */
	static void MakeRequestContext(const FGSTN_HitContextView& View, FGSTN_LinkRequestContext& OutContext);

	/** The same for another link type than the hit names (one of its definition's Alternatives) */
	static void MakeRequestContext(const FGSTN_HitContextView& View, const FGameplayTag& LinkType, FGSTN_LinkRequestContext& OutContext);

	/**
	 * The hit's attacker and victim are the two ends of one live Body link at the end of the previous frame (what LinkFilter and the partner hits
	 * ask). A direct entry (FGSTN_LinkPartnerHit, a throw) is judged on the victim's side alone (its link of that serial and type, the attacker as
	 * partner): the attacker cleared its own on the key's frame. An overlap hit needs both sides Active with each other, one serial and type
	 */
	static bool ArePartners(const FGSTN_HitContextView& View);

	/** Scales the hit's reaction away (no hitstun, knockback, stagger or flinch); damage, meter and events stay (a pummel, a hit that opts in) */
	static void SuppressReaction(FGSTN_StepResult& Out);
};
