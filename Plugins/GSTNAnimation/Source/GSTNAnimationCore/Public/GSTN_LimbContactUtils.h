// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_LimbContactTypes.h"
#include "GSTN_LimbContactUtils.generated.h"

class UWorld;
struct FCollisionQueryParams;

/**
 * Stateless rules of limb contact IK (limb-contact-ik.md). The contact rule and the alignment are pure math, shared by the Control Rig
 * unit (GSTNAnimationRig) and the tests; the probe is the only part that touches the world and runs on the game thread only.
 */
UCLASS()
class GSTNANIMATIONCORE_API UGSTN_LimbContactUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Where one limb should go and how much. All inputs in the same space (the rig's: component space).
	 *
	 *   Gap = dot(Effector - Surface.Point, Surface.Normal) - ContactOffset
	 *   Target = Effector - Surface.Normal * Gap                     (the animated location, slid along the normal onto the contact plane)
	 *   TargetAlpha = 1 at Gap <= AttachDistance, smooth fade to 0 at Gap >= ReleaseDistance
	 *               = 0 if the surface is invalid, if bRequireReach and the chain root is not farther from the surface than the effector,
	 *                 or if Target is farther than MaxReachRatio * ChainLength from the chain root
	 *
	 * Only motion along the normal is added, so the clip still decides where on the wall the hand or the foot lands
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Limb Contact")
	static FGSTN_LimbContactResult ComputeContact(const FVector& Effector, const FVector& ChainRoot, float ChainLength, const FGSTN_LimbSurface& Surface, const FGSTN_LimbContactSettings& Settings);

	/** Moves Current towards Target at BlendInSpeed (rising) or BlendOutSpeed (falling), clamped to [0, 1]. A speed of 0 snaps */
	UFUNCTION(BlueprintPure, Category = "GSTN|Limb Contact")
	static float StepContactAlpha(float Current, float Target, const FGSTN_LimbContactSettings& Settings, float DeltaSeconds);

	/**
	 * Rotation turned so that LocalAxis (in the rotated frame) points along -SurfaceNormal, by Alpha in [0, 1]. Shortest arc, so the twist
	 * around the axis stays the animated one. Returns Rotation unchanged when an input is degenerate. C++ only (FQuat)
	 */
	static FQuat AlignAxisToSurface(const FQuat& Rotation, const FVector& LocalAxis, const FVector& SurfaceNormal, float Alpha);

	/** The surface moved into the space of SpaceTransform (e.g. world to component: pass the component's world transform) */
	UFUNCTION(BlueprintPure, Category = "GSTN|Limb Contact")
	static FGSTN_LimbSurface InverseTransformSurface(const FGSTN_LimbSurface& Surface, const FTransform& SpaceTransform);

	/**
	 * Game thread only. Sweeps a sphere from Origin along -ExpectedNormal for Settings.ProbeDistance. Returns the hit (world space) when its
	 * normal is within Settings.MaxNormalAngle of ExpectedNormal, else Fallback (which may itself be invalid). Not called by simulation
	 */
	static FGSTN_LimbSurface ProbeSurface(const UWorld* World, const FVector& Origin, const FVector& ExpectedNormal, const FGSTN_LimbSurface& Fallback,
		const FGSTN_LimbProbeSettings& Settings, const FCollisionQueryParams& QueryParams);
};
