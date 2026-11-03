// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PairMoveTypes.generated.h"

/**
 * Pair-move enums (combat-m7-piggyback.md 3). The pair-move definition itself is a UObject of the Runtime module (G8). The spec's
 * EGSTN_LinkOutcome { Keep, Break, SwapRoles } is not declared here: GSTNLink ships it as EGSTN_LinkActionOutcome { KeepLink, Break,
 * SwapRoles } on FGSTN_LinkActionInfo (M7 G2), and the pair-move definition fills that field directly.
 */

/** Which side of a mount plays a pair move's initiator part */
UENUM(BlueprintType)
enum class EGSTN_PairRole : uint8
{
	Carrier,
	Rider
};

/** Who is credited for the hits and KOs of a pair move (combat-m7-piggyback.md 6.7) */
UENUM(BlueprintType)
enum class EGSTN_PairCredit : uint8
{
	/** The hitbox owner only; the partner is not named in the event */
	OwnerOnly,
	/** The hitbox owner takes the amounts; the initiator is named in the event as the credited partner */
	InitiatorAndOwner,
	/** The amounts are split between owner and partner by the definition's share (UGSTN_MountUtils::SplitCredit) */
	Split
};

/** How the partner's part of a pair move behaves */
UENUM(BlueprintType)
enum class EGSTN_PairPartnerMode : uint8
{
	/** The partner cannot refuse and a hit on it does not cancel its part; its commands are locked while it plays (GSTNLink's bPartnerLocksCommands) */
	Cooperative,
	/** The partner's part is an ordinary forced timeline: a hit reaction replaces it */
	Interruptible
};
