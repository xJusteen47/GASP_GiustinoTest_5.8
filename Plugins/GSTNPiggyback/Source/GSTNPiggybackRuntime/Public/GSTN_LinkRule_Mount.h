// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MountTypes.h"
#include "Rules/GSTN_LinkRule.h"
#include "GSTN_LinkRule_Mount.generated.h"

/**
 * The mount's request rule (combat-m7-piggyback.md 3 and 6.2): GSTNLink's shared checks, then UGSTN_MountUtils::EvaluateEligibility on values read
 * from the request context only, so the rider's prediction (the attacker step's PredictRequest), the carrier's resolution (the LinkRequest step)
 * and the interact probe's pre-filter decide alike. The initiator is the rider, the target the carrier.
 *
 * What the context gives, and what it does not (m7-doc-deltas G5):
 * - the kits: FGSTN_MountKitExtension of each side (none = refused), its CarryCapacity and the rider's Weight (the combat attributes are not on
 *   the request side);
 * - links, lock and hitstun: the sides' link states and request tags (the mount definition names Combat.State.Hitstun so the tag is in the
 *   snapshot's request mask);
 * - range: decided by the mount hitbox and the interact offer's reach, not here (the context carries no position);
 * - cooldown: the initiator's generic link cooldown (CooldownEndFrame); nothing writes a remount cooldown yet (OPEN).
 * The team is not checked here: the type lists GSTNLink's UGSTN_LinkRule_Team (AlliesOnly) before this rule.
 */
UCLASS(meta = (DisplayName = "Mount Rule"))
class GSTNPIGGYBACKRUNTIME_API UGSTN_LinkRule_Mount : public UGSTN_LinkRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LinkRuleResult Evaluate_Implementation(const FGSTN_LinkRequestContext& Context) const override;

	/** The carry ratio (rider weight / carrier capacity, 0..255) as the link's Payload, held by both sides for the carry load (G6) */
	virtual uint8 ComputePayload_Implementation(const FGSTN_LinkRequestContext& Context) const override;

	/** The eligibility input of a request context (pure; tests and the debug probe read it) */
	static FGSTN_MountEligibilityInput MakeEligibilityInput(const FGSTN_LinkRequestContext& Context);

	/** The link refusal a mount verdict is reported with (Accept = None) */
	static EGSTN_LinkRefusal ToRefusal(EGSTN_MountVerdict Verdict);
};
