// Backpacks, Network Prediction half (M11 L7c: the dome blocks hits).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"

/**
 * The attacker outcome Backpack.Dome: Ricochet when the pipeline run carries a FGSTN_BackpackDomeBlock (the BackpackDome step blocked the hit),
 * Normal otherwise. The core then ends the melee attacker's move into its recoil (the move's RicochetResponse, else the combat default shove),
 * opens no hitstop and flags the victim's inbox entry. Reads the verdict the step wrote, so the two never disagree. Pure, like every hook.
 */
class GSTNBACKPACKNP_API FGSTN_BackpackDomeAttackerOutcome : public IGSTN_AttackerOutcomeExtension
{
public:
	virtual FName GetName() const override;
	virtual EGSTN_AttackerHitOutcome Evaluate(const FGSTN_HitContextView& View) const override;
};

/**
 * The applier of FGSTN_BackpackDomeBlock: writes nothing (a blocked hit changes no state) and raises Backpack.Event.DomeBlocked with the hit's
 * event as its envelope. Through AppendEvents, which the victim step runs for a ricochet too, so a blocked melee hit and a blocked projectile
 * both publish it, once.
 */
class GSTNBACKPACKNP_API FGSTN_BackpackDomeEventApplier : public IGSTN_OutcomeExtensionApplier
{
public:
	virtual const UScriptStruct* GetExtensionStruct() const override;
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override {}
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const override;
};
