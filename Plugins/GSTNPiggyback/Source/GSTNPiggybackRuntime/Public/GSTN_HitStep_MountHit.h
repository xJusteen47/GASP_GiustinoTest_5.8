// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MountTypes.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_MountHit.generated.h"

/** The mount roles as GSTNLink's role bytes of the mount type (FGSTN_MountLinkType: the rider is the initiator) */
namespace GSTNMountRole
{
	constexpr uint8 None = 0;
	constexpr uint8 Rider = 1;
	constexpr uint8 Carrier = 2;
}

/** Everything the mount hit rule decides from, as plain values (UGSTN_HitStep_MountHit::Evaluate). Filled from the hit view by the step */
struct FGSTN_MountHitInput
{
	/** The victim's role in an active mount link (GSTNMountRole), None when it holds no mount */
	uint8 VictimRole = GSTNMountRole::None;
	/** The attacker is the victim's mount partner */
	bool bFromPartner = false;
	/** The hitbox's link filter: only LinkedPartnerOnly lets a partner hit land (M7.12) */
	EGSTN_LinkFilter LinkFilter = EGSTN_LinkFilter::Any;
	/** The mount type's hit break rule holds for the victim's role at the computed knockback (UGSTN_HitStep_LinkBreakOnHit::QualifiesForBreak) */
	bool bAboveBreak = false;
	/** The hit comes from inside the carrier's cover arc (only read for a rider victim) */
	bool bInCoverArc = false;
	bool bHasKnockback = false;
	FGSTN_KnockbackResult Knockback;
	/** The attacker's role in an active mount link (GSTNMountRole) and its partner, for the credit */
	uint8 AttackerRole = GSTNMountRole::None;
	int32 AttackerPartnerId = INDEX_NONE;
	int32 VictimEntityId = INDEX_NONE;
};

/** What the mount hit rule decides */
struct FGSTN_MountHitDecision
{
	/** The hit does not land on this victim (a partner hit without LinkedPartnerOnly, or a rider covered by its carrier) */
	bool bDrop = false;
	bool bCovered = false;
	/** The mount breaks; the reason tag (Combat.Link.BreakReason.RiderHit / .CarrierHit) */
	FGameplayTag BreakReason;
	/** The rider's knockback scaled below its break threshold (replaces the outcome's) */
	bool bScaled = false;
	FGSTN_KnockbackResult Knockback;
	/** The credit the attacker's mount gives its partner (unset for OwnerOnly) */
	FGSTN_HitCredit Credit;
};

/**
 * The MountHit step of the hit resolution pipeline (combat-m7-piggyback.md 6.7, plan G7). Runs right after ComputeKnockback, before
 * ApplyDamage, MeterGain and GSTNLink's LinkBreakOnHit:
 *
 * - Partners: a hit between mount partners lands only with LinkFilter LinkedPartnerOnly (M7.12), even with friendly fire on.
 * - Rider victim: below RiderBreakKnockback its knockback and launch are scaled by RiderKnockbackScale and its hitstun by RiderHitstunScale
 *   (it flinches, takes the damage and stays mounted); above it the computed knockback stands and the mount breaks with reason RiderHit.
 *   With CarrierCoverFrontDeg > 0 a hit coming from inside that arc around the rider's facing (which is the carrier's, turned by the rider's
 *   parked yaw) does not land on the rider: the carrier takes it only if its own hurtboxes overlap the hitbox (no redirect, m7 deltas G7).
 * - Carrier victim: a normal hit; above CarrierBreakKnockback the mount breaks with reason CarrierHit and the rider is dropped.
 * - Credit: a hit by a mounted fighter credits its partner per FGSTN_MountRules::DefaultCredit (OwnerOnly by default: no credit). Pair moves
 *   (G8) replace it with their definition's credit.
 *
 * The break is written as GSTNLink's FGSTN_LinkBreakOutcome with the plugin's reason, ahead of LinkBreakOnHit's own mark (the first reason
 * wins), so the mount names its reason and the other link types are untouched. The thresholds are the registered definition's hit breaks
 * (the same numbers LinkBreakOnHit reads); the scales and the credit come from the rules asset's FGSTN_MountRules extension, or its defaults.
 *
 * Pure, like every step: the victim's and the attacker's link snapshot extensions and the outcome in, the result out.
 */
UCLASS(meta = (DisplayName = "Mount Hit"))
class GSTNPIGGYBACKRUNTIME_API UGSTN_HitStep_MountHit : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;

	/** The rule as a function of plain values (tests and the step) */
	static FGSTN_MountHitDecision Evaluate(const FGSTN_MountHitInput& Input, const FGSTN_MountRules& Rules);

	/** Whether a hit travelling along HitboxToVictim comes from inside an arc of ArcDeg centred on FacingYaw (degrees about +Z) */
	static bool IsInFrontArc(const FVector& HitboxToVictim, float FacingYaw, float ArcDeg);

	/** The match's mount rules: the rules asset's FGSTN_MountRules extension, else the defaults */
	static const FGSTN_MountRules& FindRules(const UGSTN_CombatRules* Rules);
};
