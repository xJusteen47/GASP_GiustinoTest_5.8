// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GSTN_LimbContactTypes.generated.h"

/**
 * Limb contact IK (limb-contact-ik.md): a hand or a foot that the animation brings close to a surface (a wall while sliding or climbing,
 * later a ledge or a ladder) is pulled onto it, so it touches the surface instead of floating in front of it or sinking into it.
 *
 * Presentation only. The surface is probed on the game thread from the last evaluated pose, the IK runs in a Control Rig on the anim
 * worker thread, and nothing here is read by simulation: Mover, abilities, timelines, hitboxes and hurtboxes (which come from the
 * reference montages and baked sockets, never from the live pose) are the same with or without it.
 *
 * Spaces: everything handed to the rig is in component space, which is the rig's global space.
 */

/** One surface a limb may touch: a point on it and its normal (pointing away from the surface, towards the character) */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_LimbSurface
{
	GENERATED_BODY()

	/** False when there is nothing to touch: the limb keeps its animated pose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact")
	bool bValid = false;

	/** A point on the surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact")
	FVector Point = FVector::ZeroVector;

	/** Unit normal of the surface at Point, pointing out of it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact")
	FVector Normal = FVector::ForwardVector;
};

/** How a limb decides that it touches a surface and how it is pulled onto it. One per limb (hands and feet differ in offset and axis) */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_LimbContactSettings
{
	GENERATED_BODY()

	/** Distance kept between the effector bone and the surface: half the thickness of the hand or the foot at that bone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ContactOffset = 3.f;

	/** Gap (distance minus ContactOffset) at or under which the limb is fully on the surface. A limb inside the surface is always fully on it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float AttachDistance = 8.f;

	/**
	 * Gap beyond which the animated limb is left alone: only the limbs the animation already brings near the surface are attached,
	 * so a hand raised away from the wall stays where the clip puts it. Between AttachDistance and this, the pull fades smoothly
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ReleaseDistance = 25.f;

	/**
	 * Only attach a limb that reaches towards the surface: its effector must be closer to the surface than the root of its chain
	 * (shoulder, hip). Avoids pulling an elbow-out hand or a tucked foot backwards onto the wall
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact")
	bool bRequireReach = true;

	/**
	 * The target must be within this fraction of the chain length from the chain root, or the limb is released: a target the limb
	 * cannot reach would straighten it fully, which reads worse than a small gap
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float MaxReachRatio = 1.f;

	/** How fast the contact weight rises towards its target (per second). Smooths the moment a limb lands on the surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "1/s"))
	float BlendInSpeed = 12.f;

	/** How fast the contact weight falls (per second): leaving the wall, or a limb that the clip lifts away */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "1/s"))
	float BlendOutSpeed = 8.f;

	/** How much the effector also turns so that ContactAxis points into the surface (palm or sole flat on it). 0 = keep the animated rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ClampMax = "1"))
	float RotationAlpha = 0.6f;

	/**
	 * Axis of the effector bone, in the bone's own space, that should point into the surface: the palm direction for a hand, the sole
	 * direction for a foot. Depends on the skeleton; check it on the mesh (limb-contact-ik.md, "Tuning")
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact")
	FVector ContactAxis = FVector(0.f, 1.f, 0.f);
};

/** What the contact rule decides for one limb in one frame, before the weight is smoothed over time */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_LimbContactResult
{
	GENERATED_BODY()

	/** Desired contact weight this frame, [0, 1]: 1 on the surface, 0 left alone */
	UPROPERTY(BlueprintReadOnly, Category = "GSTN|Limb Contact")
	float TargetAlpha = 0.f;

	/** Where the effector goes at full weight: its animated location moved along the normal to ContactOffset from the surface */
	UPROPERTY(BlueprintReadOnly, Category = "GSTN|Limb Contact")
	FVector TargetLocation = FVector::ZeroVector;

	/** Signed distance of the animated effector from the surface, minus ContactOffset (negative = inside) */
	UPROPERTY(BlueprintReadOnly, Category = "GSTN|Limb Contact")
	float Gap = 0.f;
};

/** How the game thread looks for the surface under each limb */
USTRUCT(BlueprintType)
struct GSTNANIMATIONCORE_API FGSTN_LimbProbeSettings
{
	GENERATED_BODY()

	/** Trace channel of the probe. Walls the character can slide on must block it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Length of the probe from the chain root (shoulder, hip) towards the surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProbeDistance = 120.f;

	/** Radius of the probe sphere: a little width so gaps between wall pieces do not lose the surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProbeRadius = 4.f;

	/**
	 * A hit whose normal turns more than this from the expected normal (the wall the movement mode is on) is not the same surface
	 * (a ledge top, a pipe, a corner face): the limb uses the fallback plane instead
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GSTN|Limb Contact", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float MaxNormalAngle = 40.f;
};
