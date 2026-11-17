// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

/** A floor plane: a point on it and its unit normal (pointing out of the floor) */
struct GSTNANIMATIONCORE_API FGSTN_GroundPlane
{
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;

	/** Signed height of P above the plane */
	double Height(const FVector& P) const { return FVector::DotProduct(P - Point, Normal); }
};

/**
 * The pure math of the ground contact correction (FGSTN_AnimNode_GroundContactFix), no pose or world, for the tests.
 * The correction only removes penetration: a sphere that does not sink is never moved.
 */
struct GSTNANIMATIONCORE_API FGSTN_GroundContactSolver
{
	/** How much a sphere of Radius around P sinks into the plane (0 when it does not) */
	static double Penetration(const FGSTN_GroundPlane& Plane, const FVector& P, double Radius)
	{
		return FMath::Max(0.0, Radius - Plane.Height(P));
	}

	/** Lift (along the normal) that takes every body sphere out of the floor, capped at MaxLift */
	static double RequiredLift(const FGSTN_GroundPlane& Plane, TConstArrayView<FVector> Points, TConstArrayView<double> Radii, double MaxLift);

	/** The lift goes up at once and comes down towards Required with HalfLife (0 = at once) */
	static double SmoothLift(double Current, double Required, double DeltaTime, double HalfLife);

	/** Moves Weight towards Target over BlendIn (rising) or BlendOut (falling) seconds; 0 = at once */
	static float BlendWeight(float Weight, float Target, float DeltaTime, float BlendIn, float BlendOut);

	/**
	 * Where a limb end should go after the body lift. Original = before the lift, Lifted = after it.
	 * - It touched or sank before the lift (height < Radius + ContactBand) and bKeepContact: back to Original, raised out of the floor.
	 * - Otherwise, if it still sinks after the lift: raised out of the floor from Lifted.
	 * - Otherwise: no IK (returns false).
	 * The move from Lifted is capped at MaxCorrection.
	 */
	static bool LimbTarget(const FGSTN_GroundPlane& Plane, const FVector& Original, const FVector& Lifted, double Radius, double ContactBand,
		bool bKeepContact, double MaxCorrection, FVector& OutTarget);
};
