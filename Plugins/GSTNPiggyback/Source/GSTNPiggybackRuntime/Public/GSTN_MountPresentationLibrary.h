// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_MountPresentationLibrary.generated.h"

/**
 * Where a rider's hands and feet reach on its carrier this frame (combat-m7-piggyback.md 6.8, M7 G10): world transforms of the carrier's
 * mesh sockets, for the rider's animation blueprint (two-bone IK or a full-body IK goal). Presentation only.
 */
USTRUCT(BlueprintType)
struct GSTNPIGGYBACKRUNTIME_API FGSTN_MountIKTargets
{
	GENERATED_BODY()

	/** Both hand sockets exist on the carrier's mesh */
	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	bool bHands = false;

	/** Both foot sockets exist on the carrier's mesh */
	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	bool bFeet = false;

	/** IK weight: the rider's attach slide (0 when it starts, 1 seated), so the limbs reach the seat as the body does */
	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	float Alpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	FTransform HandL = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	FTransform HandR = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	FTransform FootL = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Piggyback|IK")
	FTransform FootR = FTransform::Identity;
};

/**
 * Presentation helpers of a mount, for animation blueprints and cameras. They read what this machine shows (the link presentation component,
 * the carrier's animated mesh), never the simulation, so they may differ between machines by the interpolation of the partner.
 */
UCLASS()
class GSTNPIGGYBACKRUNTIME_API UGSTN_MountPresentationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Socket base names used when the rider's RiderProfile names none: the carrier sockets are <Base>L and <Base>R */
	static const FName DefaultHandSocketBase;
	static const FName DefaultFootSocketBase;

	/**
	 * The rider's IK targets on its carrier: false (Out cleared) unless Rider is shown seated as a mount's rider on this machine. Socket names
	 * come from the rider kit's RiderProfile (RiderPoseSocketHands / Feet as a base, suffixed L and R), the defaults GSTN_MountHand /
	 * GSTN_MountFoot otherwise; a pair missing on the carrier's mesh leaves its flag off
	 */
	UFUNCTION(BlueprintPure, Category = "Piggyback|Presentation")
	static bool GetRiderIKTargets(const AActor* Rider, FGSTN_MountIKTargets& Out);
};
