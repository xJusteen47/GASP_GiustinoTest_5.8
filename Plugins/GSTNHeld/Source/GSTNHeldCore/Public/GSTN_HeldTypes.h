// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Movement/GSTN_BallisticMode.h"

#include "GSTN_HeldTypes.generated.h"

GSTNHELDCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogGSTNHeld, Log, All);

/** Where an object is in its life. At rest it is Idle again (after a landing too): there is no separate Landed state yet */
UENUM(BlueprintType)
enum class EGSTN_HeldObjectState : uint8
{
	/** At rest in the world, dormant, can be picked up */
	Idle,
	/** In a holder's hand: drawn at the holder on every machine */
	Held,
	/** Thrown: drawn with its projectile until the projectile hits, lands or expires */
	Flying
};

/**
 * What one object is worth to a holder and a thrower (M10 slice 1 subset of the plan's UGSTN_ObjectDefinition: no size class, weight,
 * drop-on-hit rule or impact behaviour yet). Every default is a TEST VALUE until the owner sets real kits (handover m10-slice1).
 */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldObjectTuning
{
	GENERATED_BODY()

	FGSTN_HeldObjectTuning();

	/** A holder whose snapshot position is within this distance of the object can pick it up. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Pickup", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PickupRadius = 200.f;

	/**
	 * Where the object sits in the holder's actor space while held (X forward, Z up, from the capsule centre). Presentation only: the
	 * plan's spike S2 decides between this root offset and a hand socket. TEST VALUE
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Hold", meta = (ForceUnits = "cm"))
	FVector HoldOffset = FVector(45.0, 0.0, 70.0);

	/** Half height of the object at rest: a landed object is placed this far above the floor it lands on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Rest", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RestHalfHeight = 20.f;

	/**
	 * The throw as a GSTNProjectile shot: speed, gravity, lifetime, hit shape, hit data, max hits. Origin, direction and owner are filled
	 * by the throw ability. Damage, knockback, speed and arc are TEST VALUES (the owner sets the feel and the numbers of real kits)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Throw")
	FGSTN_ProjectileParams Throw;
};

/** One pickup request the host received for a frame (a holder asking, from where) */
struct FGSTN_PickupRequest
{
	/** Combat entity id of the holder: the race tie-break after the frame */
	int32 HolderId = INDEX_NONE;
	/** The simulation frame the request was made on (the earliest wins) */
	int32 Frame = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
};

/** One object that could be picked up */
struct FGSTN_PickupCandidate
{
	/** Stable id of the object on the host (registration order): the tie-break between two objects at the same distance */
	int32 ObjectId = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	bool bAvailable = true;
};

struct FGSTN_PickupAssignment
{
	int32 HolderId = INDEX_NONE;
	int32 ObjectId = INDEX_NONE;

	bool operator==(const FGSTN_PickupAssignment& Other) const { return HolderId == Other.HolderId && ObjectId == Other.ObjectId; }
};

/** Stateless rules of held objects */
struct GSTNHELDCORE_API FGSTN_HeldRules
{
	/**
	 * The pickup race (plan default M10.5): requests are served by earliest frame, then lower holder id; each takes the nearest available
	 * object within that object's radius (equal distance: lower object id); a taken object is gone for the next request. A holder gets
	 * at most one object (its first request). Same inputs, same assignments, in the order served
	 */
	static void ResolvePickups(TConstArrayView<FGSTN_PickupRequest> Requests, TConstArrayView<FGSTN_PickupCandidate> Candidates,
		TArray<FGSTN_PickupAssignment>& OutAssignments);

	/** Where a held object is drawn: the holder's transform with the hold offset applied in its space (rotation = the holder's) */
	static FTransform HoldTransform(const FTransform& Holder, const FVector& HoldOffset);
};
