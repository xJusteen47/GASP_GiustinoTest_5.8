// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"

/**
 * Yaw measured about the local up (= -gravity), never about world Z (game-design-direction §2d, gravity audit 2026-11-16).
 * The gravity frame is the one Mover uses (UMoverComponent::WorldToGravityTransform = FindBetweenNormals(world up, Up)), so a facing
 * yaw written here is the yaw Mover's walking mode keeps (UMovementUtils::ApplyGravityToOrientationIntent). Under world-Z gravity every
 * function returns exactly what the old world-yaw code did (fast path, no quaternion round trip).
 */
struct GSTNCOMBATCORE_API FGSTN_GravityFrame
{
	/** True when Up is world up (the fast path: the old world-yaw maths, bit for bit) */
	static bool IsWorldUp(const FVector& Up);

	/** World -> gravity frame rotation, as Mover builds it; identity for world up or a zero vector */
	static FQuat WorldToGravity(const FVector& Up);

	/** Yaw (degrees) of a world direction about Up; the part along Up is ignored */
	static float YawOfDirection(const FVector& WorldDirection, const FVector& Up);

	/** Yaw (degrees) about Up of a world rotation's forward */
	static float YawOfRotation(const FQuat& WorldRotation, const FVector& Up);

	/** Same for a rotator (world up: its Yaw as stored, no quaternion round trip) */
	static float YawOfRotator(const FRotator& WorldRotation, const FVector& Up);

	/** The world direction, perpendicular to Up, with this yaw about Up */
	static FVector DirectionOfYaw(float YawDeg, const FVector& Up);

	/** WorldRotation with its yaw about Up replaced; its gravity-relative pitch and roll stay */
	static FRotator WithYaw(const FRotator& WorldRotation, float YawDeg, const FVector& Up);

	/** The rotation that only turns about Up by the rotation's yaw about Up (a capsule standing on the gravity floor) */
	static FQuat UprightRotation(const FQuat& WorldRotation, const FVector& Up);

	/** Earth gravity (cm/s^2) authored data is written against (a throw's -980 on Z) */
	static constexpr double NominalGravity = 980.0;

	/**
	 * An acceleration authored under earth gravity (world -Z, NominalGravity) carried into the entity's own gravity: turned into its gravity
	 * frame and scaled by its strength. Earth gravity returns Authored exactly; zero gravity returns zero
	 */
	static FVector FollowGravity(const FVector& Authored, const FVector& EntityGravity);

	/** The part of V perpendicular to Up (V itself for a zero Up) */
	static FVector Flatten(const FVector& V, const FVector& Up);
};
