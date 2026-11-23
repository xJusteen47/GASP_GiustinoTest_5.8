// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "GSTN_GrabHoldTypes.generated.h"

/**
 * Where a grabber holds its grabbed partner (grab-link, owner 2026-11-23: "tiene il nemico per il collo", Ganondorf style, one hand, the enemy in
 * front and lifted). The partner's NECK is put on HoldPointCm, a point in the grabber's root space (the capsule's centre, X forward, Z up), and the
 * partner faces back toward the grabber by the grab type's own SocketOffset rotation.
 *
 * Why a point in root space and not the live hand: the parked placement must be the same on every machine and in every re-simulation, so it reads
 * only data and the REFERENCE-pose sockets FGSTN_LinkAttach bakes (m6 spec 7.3), never an animated bone. The hold animation is authored so the hand
 * reaches this point (rule 13e: the data moves, the pose is not bent to it). The result is written once per link as the parked side's AnchorOverride
 * (UGSTN_LinkMath::NeckHoldAnchor), so the snapshot, the watchdog's leash and the presentation all use it unchanged.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_GrabHoldPose
{
	GENERATED_BODY()

	/** Off = the grab type's own SocketOffset, as before this checkpoint */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Hold")
	bool bHoldByNeck = true;

	/**
	 * The hand's hold point in the grabber's root space (cm). TEST VALUE, to be matched to the grab idle pose once it exists: about the Overlord's
	 * right hand stretched in front at shoulder height. Its Z decides how high the partner is lifted (its feet leave the ground when Z is above the
	 * partner's own neck height above its root)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Hold", meta = (EditCondition = "bHoldByNeck", ForceUnits = "cm"))
	FVector HoldPointCm = FVector(60.0, 15.0, 55.0);

	/** Added to the partner's root after the neck is placed (cm, grabber root space): a per-kit nudge without moving the hand point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Hold", meta = (EditCondition = "bHoldByNeck", ForceUnits = "cm"))
	FVector ExtraOffsetCm = FVector::ZeroVector;
};

/**
 * What a GRABBED entity does to its grabber when it mashes free (grab-link, owner 2026-11-23: "per i nemici PVE conviene che facciano danno quando si
 * liberano"). Data of the grabbed entity's kit, so it is per target type: OFF by default, hence off for every player kit; a PVE enemy kit turns it on.
 * On the escape the watchdog (the only break authority) posts Hit to the grabber as a direct inbox entry from the grabbed, stamped on the break frame,
 * like a throw: no hitbox, no overlap, the normal hit pipeline (damage, knockback, hitstun) runs on it.
 */
USTRUCT(BlueprintType)
struct GSTNLINKCORE_API FGSTN_GrabEscapeHit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Escape")
	bool bDamageGrabberOnEscape = false;

	/** TEST VALUES when switched on (a light pop, Smash-like break-out push without damage scaling); the owner tunes them per enemy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grab|Escape", meta = (EditCondition = "bDamageGrabberOnEscape"))
	FGSTN_HitData Hit;

	FGSTN_GrabEscapeHit();
};
