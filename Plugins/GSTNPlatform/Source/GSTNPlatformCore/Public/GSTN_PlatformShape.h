// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformShape.generated.h"

/** Primitive of a platform shape (many-enemies-replication.md 4.2). Convex is not supported yet (see the README) */
UENUM(BlueprintType)
enum class EGSTN_PlatformShapeType : uint8
{
	/** Half extents in Extent. The only primitive with a flat top, so the only one a Stand shape may use */
	Box,
	/** Radius in Extent.X */
	Sphere,
	/** Radius in Extent.X, half height including the hemispheres in Extent.Z (the Unreal convention); axis = local Z */
	Capsule
};

namespace GSTNPlatform
{
	/** Shape ids are 0..MaxShapes-1, so the shape mask of a platform fits a uint8 (traversal-surfaces.md 4.1: cap raised to 8 for parts) */
	inline constexpr int32 MaxShapes = 8;

	/** Name of the collider of a shape id: "Surface0".."Surface7". Identical on every machine, so Mover can reference it as a movement base */
	GSTNPLATFORMCORE_API FName GetShapeComponentName(int32 ShapeId);
}

/**
 * One collider of a platform, authored in the enemy's frame (the owner actor's root, unscaled). The collider it becomes is a
 * kinematic, Movable, unattached primitive posed at the platform's pose point, never a bone, never the animated mesh, never the
 * enemy's capsule (many-enemies-replication.md 4.3).
 *
 * Static configuration: identical on every machine, never written at runtime (BlueprintReadOnly).
 */
USTRUCT(BlueprintType)
struct GSTNPLATFORMCORE_API FGSTN_PlatformShape
{
	GENERATED_BODY()

	/** Stable id 0..7: the bit of this shape in the shape mask and the suffix of its collider's name (Surface<Id>) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0", ClampMax = "7"))
	uint8 ShapeId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	EGSTN_PlatformShapeType Type = EGSTN_PlatformShapeType::Box;

	/** Box: half extents. Sphere: X is the radius. Capsule: X is the radius, Z the half height including the hemispheres (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	FVector Extent = FVector(50.0, 50.0, 10.0);

	/** Centre of the shape in the enemy's frame (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ForceUnits = "cm"))
	FVector LocalLocation = FVector::ZeroVector;

	/** Rotation of the shape in the enemy's frame. There is no scale: a scaled collider is one more number to disagree on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FRotator LocalRotation = FRotator::ZeroRotator;

	/** The shape's transform in the enemy's frame */
	FTransform GetLocalTransform() const { return FTransform(LocalRotation, LocalLocation); }

	/** The shape's world transform for an enemy pose (the pose's scale is ignored) */
	FTransform ComposeWorld(const FTransform& OwnerPose) const;
};
