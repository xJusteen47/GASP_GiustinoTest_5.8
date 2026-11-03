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

/**
 * The velocity a rider leaves a pair move's Break with (combat-m7-piggyback.md 6.6, M7 G9): a launch along the exit's aim pitched up, a
 * straight up part, and a share of the staying partner's velocity. All zero (the default) = a plain put-down. Content, read by the mount
 * placer from the query only, so the watchdog and an initiator placing its own exit compute the same numbers from the same inputs.
 */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKCORE_API FGSTN_PairExitLaunch
{
	GENERATED_BODY()

	/** Speed along the aim (the carrier's forward on the movement plane) pitched up by PitchDeg. The throw's ThrowSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair Move|Launch", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float Speed = 0.f;

	/** Pitch of the aimed part above the movement plane. The throw's ThrowAngleDeg */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair Move|Launch", meta = (ClampMin = "-89.0", ClampMax = "89.0", ForceUnits = "deg"))
	float PitchDeg = 0.f;

	/** Speed straight along the local up, added to the aimed part. The boost jump's launch (rider jump velocity x boost multiplier) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair Move|Launch", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float UpSpeed = 0.f;

	/** Share of the staying partner's velocity added on top (ThrowInheritCarrierVelocity, InheritCarrierVelocity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair Move|Launch", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float InheritPartnerVelocity = 0.f;

	/** Inherit only the partner's velocity on the movement plane (the boost jump: the carrier's fall or jump does not lift the rider) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pair Move|Launch")
	bool bInheritHorizontalOnly = false;

	bool IsZero() const { return Speed == 0.f && UpSpeed == 0.f && InheritPartnerVelocity == 0.f; }
};
