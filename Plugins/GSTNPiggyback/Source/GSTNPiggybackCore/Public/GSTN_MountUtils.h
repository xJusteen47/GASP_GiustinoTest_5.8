// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MountTypes.h"
#include "GSTN_PairMoveTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_MountUtils.generated.h"

/**
 * The mount's pure math and rules (combat-m7-piggyback.md 3): stateless, in doubles, no world, no actor, no tag lookup. Identical on every
 * machine for identical inputs, so the rider's prediction and the carrier's resolution, the carry load on server and client, and the exit
 * candidates of a predicted and an authoritative dismount all agree. The Runtime module only gathers inputs and applies results.
 */
UCLASS()
class GSTNPIGGYBACKCORE_API UGSTN_MountUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The verdict of a mount request: the first failing check, in this order (the kits first, so a kit that can never mount reports that
	 * whatever else holds; the range and weight last, as the checks a player can fix by moving or by choosing another carrier):
	 * NotAllowedKit, RiderIneligible, AlreadyLinked (carrier, then rider), Cooldown, CarrierHitstun, CarrierBusyLock, OutOfRange,
	 * WeightTooHigh; Accept when none fails. Boundaries: weight equal to the capacity, distance equal to the range and a frame equal to
	 * the cooldown's end frame are accepted.
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Mount")
	static EGSTN_MountVerdict EvaluateEligibility(const FGSTN_MountEligibilityInput& Input);

	/**
	 * The rider's load on the carrier, RiderWeight / MaxCarryWeight clamped to 0..1 and quantised to 0..255 (round half up), the link's
	 * Payload byte. A capacity of zero or less carries nothing: any positive weight is a full load (255), no weight is 0. A NaN weight is 0.
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Carry")
	static uint8 ComputeCarryRatio(double RiderWeight, double MaxCarryWeight);

	/** A quantised carry ratio as 0..1 (exactly 0 and 1 at the ends) */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Carry")
	static double CarryRatioToUnit(uint8 RatioQ);

	/** The penalties at a quantised ratio: each fraction scaled linearly, the air jumps scaled and rounded toward zero (all of them at 255) */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Carry")
	static FGSTN_CarryTuning ScaleCarryTuning(const FGSTN_CarryTuning& AtFullLoad, uint8 RatioQ);

	/**
	 * The rider's anchor in the world: the carrier's transform, then the socket relative to the carrier, then the mount offset relative to the
	 * socket, then the rider's own turn of LocalYawDeg about the anchor's up axis (the turn does not move the anchor's root). The same
	 * composition as GSTNLink's parked placement followed by UGSTN_LinkMath::ComposeLocalYaw. SocketLocal is identity for a root mount.
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Anchor")
	static FTransform ComposeAnchor(const FTransform& CarrierTransform, const FTransform& SocketLocal, const FTransform& MountOffset, double LocalYawDeg);

	/**
	 * Where a rider may be put down, in the order GSTNLink's exit placer tries them (first free wins). Every candidate except the anchor sits
	 * DistanceCm from the carrier's root along one of the carrier's own axes (back -X, left -Y, right +Y, front +X: on the carrier's plane,
	 * so any gravity direction works) and faces the carrier's way; the anchor candidate is the Anchor itself. Orders by mode:
	 * BehindCarrier back, left, right, front; SideOfCarrier right, left, back, front; AtAnchor anchor, back, left, right, front.
	 * A negative distance counts as zero.
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Exit")
	static TArray<FTransform> ExitCandidates(const FTransform& Anchor, const FTransform& CarrierTransform, EGSTN_ExitMode Mode, double DistanceCm);

	/**
	 * The exit velocity of Launch (M7 G9): Speed along Aim flattened onto the plane normal to Up and pitched up by PitchDeg, plus UpSpeed along
	 * Up, plus InheritPartnerVelocity x PartnerVelocity (its in-plane part only with bInheritHorizontalOnly). An Aim along Up or zero drops
	 * the aimed part. Up is normalised; a zero Up counts as world up.
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Exit")
	static FVector ComputeExitLaunch(const FGSTN_PairExitLaunch& Launch, const FVector& Aim, const FVector& Up, const FVector& PartnerVelocity);

	/** A fraction (0..1, clamped) as a partner share byte, round half up: 0.5 is 128 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Credit")
	static uint8 MakePartnerShare(double Fraction);

	/**
	 * Amount split between the hitbox owner and the partner: the partner gets Amount * PartnerShare / 255, the owner Amount minus that, so
	 * nothing is created or lost by the split (share 0 = all to the owner, 255 = all to the partner, both exactly).
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Credit")
	static FGSTN_CreditSplit SplitCredit(uint8 PartnerShare, double Amount);
};
