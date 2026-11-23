// Spin and drill plugin, module GSTNSpinDrillGAS.

#pragma once

#include "CoreMinimal.h"

class UGSTN_LinkTypeDefinition;
class UObject;

/**
 * The drill latch as a GSTNLink type made by code (plan D9, the mount's pattern): Combat.Link.Type.Drill on the Body channel, built at
 * PostEngineInit and added to the link registry, so no asset is needed (the owner left asset vs code open: code first, an asset via a setup
 * script can replace it later with the same values).
 *
 *  - Role 1, Driller: parked on the target (no velocity, no gravity, placed from the target), acts while linked (the bore's hits), no
 *    collision with the target, YawLimitDeg 0 (the drill does not turn with the stick, unlike the latch's 360 default). Parked at a FIXED offset
 *    above the target's root (DrillerOffset): GSTNLink's role placement is per type, not per latch, so the tip does not sit where it hit yet
 *    (OPEN: a per-link anchor needs a GSTNLink seam).
 *  - Role 2, Drilled: not parked, acts while linked (a moving enemy keeps walking: D10 for free through the parked transform).
 *  - Request: UGSTN_LinkRule_Drill, and Combat.Immunity.Drill in RequestRequirements' IgnoreTags (naming it there also puts it in the snapshot's
 *    request tag mask, which is how the rule can read it). Refused -> the hit does not land and the dive bounces.
 *  - bInitiatorPredicts, PendingFrames 2: the driller's own machine seats on the frame the authority's response would arrive (7.5: the owning
 *    client predicts the latch; a refusal it could not see comes back as one rollback).
 *  - Breaks: a hit on the driller (any knockback; the target's own hits do not) detaches it (7.4: the hitstun wins), the target's death ends
 *    it (the drill launches: target gone), leash LeashCm. No timeout: the drill releases the link itself when its bore ends.
 */
struct GSTNSPINDRILLGAS_API FGSTN_DrillLinkType
{
	/** Driller placement over the target's root (cm); placeholder until the per-link anchor */
	static FTransform DrillerOffset() { return FTransform(FVector(0.f, 0.f, 100.f)); }

	/** Distance from the expected placement before the link breaks (Distance) */
	static constexpr float LeashCm = 300.f;

	/** A new definition with the values above, outered to Outer (the transient package when null). Pure: tests build their own */
	static UGSTN_LinkTypeDefinition* Build(UObject* Outer);

	/** Builds the definition once and adds it to the link registry; safe to call twice */
	static void Register();

	static void Unregister();

	/** The registered definition, or null before Register */
	static const UGSTN_LinkTypeDefinition* Get();
};
