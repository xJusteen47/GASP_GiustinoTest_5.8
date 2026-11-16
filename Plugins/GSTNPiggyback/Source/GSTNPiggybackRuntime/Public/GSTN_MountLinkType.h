// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MountTypes.h"

class UGSTN_LinkTypeDefinition;

/**
 * The mount link type (combat-m7-piggyback.md 2b(1), plan G5), made by code and added to GSTNLink's registry as GSTNExtensionTest adds its carry: a
 * data asset would need the editor to author its instanced rules and placer, and the definition is fully decided by FGSTN_MountRules and the
 * constants below, so code is the single source (m7-doc-deltas G5).
 *
 * Shape: Body channel; role 1 = Rider (the initiator: parked on the carrier's root + RiderOffset, acts, no collision with the carrier, turns
 * freely, its Release reason is Dismount); role 2 = Carrier (the target: acts, its Release reason is Drop). bInitiatorPredicts with
 * PendingFrames from the rules (2): the rider shows itself mounted two frames after its request on its own machine, a refusal costs one
 * rollback there. Rules: GSTNLink's team rule (AlliesOnly, unless the rules say otherwise), then UGSTN_LinkRule_Mount. Hit breaks: the carrier above
 * CarrierBreakKnockback, the rider above RiderBreakKnockback, partner hits never. Deaths break it; no leash (LeashCm 0). Exit: UGSTN_Placer_Mount
 * (BehindCarrier). Interact offer Interact.Target.Mount at priority 50 (below the grab's 100: an enemy and an ally in reach = grab, INT.5),
 * reach 150 cm, for kits with FGSTN_MountKitExtension.
 */
struct GSTNPIGGYBACKRUNTIME_API FGSTN_MountLinkType
{
	/** The rider's parked place relative to the carrier's root (the capsule centre): behind and above, no socket needed on the skeleton */
	static FTransform RiderOffset() { return FTransform(FVector(-20.0, 0.0, 95.0)); }

	/** The interact offer's priority, below GSTNLink's grab (100) */
	static constexpr int32 OfferPriority = 50;

	/** The interact offer's reach */
	static constexpr float OfferReachCm = 150.f;

	/**
	 * The leash, 0 = none (G6). GSTNLink measures it from the parked rider's CAPSULE, which the parked mode never moves (only the snapshot and the
	 * presentation ride the carrier), so any leash broke the mount once the carrier walked that far (300 cm in G5). A carrier teleport
	 * (FGSTN_MountRules::MaxCarrierTeleportCm) needs its own check: OPEN
	 */
	static constexpr float LeashCm = 0.f;

	/** A new definition of the mount type from Rules, owned by Outer (not registered) */
	static UGSTN_LinkTypeDefinition* Build(UObject* Outer, const FGSTN_MountRules& Rules);

	/** Builds the definition with the default rules and adds it to GSTNLink's registry (module startup, once the engine is up). Idempotent */
	static void Register();

	/** Removes it from the registry (module shutdown) */
	static void Unregister();

	/** The registered definition, or null before Register */
	static const UGSTN_LinkTypeDefinition* Get();

	/** The plugin's own break reasons, in the order they are registered with GSTNLink (indices from EGSTN_LinkBreakReason::FirstPlugin, by tag name) */
	static TArray<FName> GetBreakReasonNames();
};
