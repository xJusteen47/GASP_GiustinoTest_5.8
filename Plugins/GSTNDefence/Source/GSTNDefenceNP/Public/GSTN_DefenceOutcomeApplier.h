// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"

/**
 * Applies the defence steps' outcomes (FGSTN_DefenceHitOutcome) to the victim's defence fragment when the victim step applies a hit, at the
 * hit's frame plus one (m6 spec 5.3, 5.4): the only writer of the hit-driven defence state.
 *
 *  - Parried: LastParryFrame = the hit frame (the OnParry cancel condition); source Shield: ParryRecoveryEndFrame = the hitstop end plus the
 *    profile's DefenderRecoveryFrames (the tick extension ends the shield timeline there); source Move: the window's new end.
 *  - Blocked: the value re-stamped at the hit frame, ShieldStunEndFrame = hitstop end + stun (the longer of two), the push back queued from
 *    the hitstop end (the tick extension gives it to Mover). The mirror is re-written on the next tick.
 *  - Broken: Breaking, not Raised, value 0, BreakEndFrame = hit frame + BreakStunFrames, and a reaction request: the break launch, up,
 *    no tumble (the victim step makes it a launch with its hitstun; the shield timeline is cancelled by that stagger).
 *
 * AppendEvents raises Combat.Event.Parried (magnitude = EGSTN_ParrySource), Combat.Event.Blocked or Combat.Event.ShieldBroken with the hit's
 * event as the envelope, on every machine and re-simulation; the authority publishes them once.
 *
 * Deterministic: the outcome's integers and the fragment only; no world, no component.
 */
class GSTNDEFENCENP_API FGSTN_DefenceOutcomeApplier : public IGSTN_OutcomeExtensionApplier
{
public:
	virtual const UScriptStruct* GetExtensionStruct() const override;
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override;
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const override;
};
