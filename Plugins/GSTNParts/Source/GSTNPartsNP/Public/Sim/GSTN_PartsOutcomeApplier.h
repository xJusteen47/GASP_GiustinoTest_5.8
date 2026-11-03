// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"

/**
 * Applies the outcome of a part hit at the victim's tick (combat-parts.md §5.3, §5.4). The part resolve step only DECIDES: it hands this
 * applier an FGSTN_PartHitOutcome with the whole state the hit produced, and this is the only code that writes the parts fragment.
 *
 *  - Apply writes mask, damage bytes and serial into the victim's fragment at F+1 with one assignment, so a re-simulated hit lands on
 *    the same bytes. The mask is monotonic: bits already in the fragment are kept even if the outcome was built from an older view.
 *    Two entries of one drain are applied in the inbox order; the drain rebuilds the parts extension between them from the fragment
 *    this wrote (the snapshot contributor), so the second hit is resolved against what the first left.
 *  - AppendEvents makes the Combat.Event.Part.* events of the hit with the hit's own event as envelope (the payload, part and serial
 *    are packed by the P5 events). Events are appended on every machine and every re-simulation, like the hit events; what keeps a
 *    consumer from playing a break twice is the key (entity, part, serial) of FGSTN_PartEvents::MakeKey, which a redelivered hit
 *    reproduces exactly.
 *
 * Break effects: the derived ones (tags, hit groups, stats) are projections of the mask made by the tick extension. The one-shot
 * launch and stagger of a break are a reaction request on the apply context (FGSTN_ReactionRequest): this applier collects them for the
 * parts the hit newly broke and the victim step merges them into the hit's own reaction, as a hit of that strength would (P8).
 *
 * No world, no component, no net mode: its only inputs are the extension, the view and the victim's fragment array.
 */
class GSTNPARTSNP_API FGSTN_PartsOutcomeApplier : public IGSTN_OutcomeExtensionApplier
{
public:
	virtual const UScriptStruct* GetExtensionStruct() const override;
	virtual void Apply(const FInstancedStruct& Extension, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const override;
	virtual void AppendEvents(const FInstancedStruct& Extension, const FGSTN_SimEvent& Envelope, FGSTN_SimEventOutbox& Outbox) const override;
};
