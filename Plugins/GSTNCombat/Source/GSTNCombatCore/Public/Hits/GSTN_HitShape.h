// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_HitShape.generated.h"

/** The three primitives hitboxes and hurtboxes are made of (combat-m4-hits 3) */
UENUM(BlueprintType)
enum class EGSTN_ShapeType : uint8
{
	Sphere,
	Capsule,
	Box
};

/**
 * One primitive in the local space of whatever carries it (a hitbox's space, a hurtbox's root). All math is in doubles and
 * uses only IEEE operations on the inputs, so the same inputs give the same bits on every machine and in a resimulation.
 *
 * Extent by type: Sphere uses X as the radius; Capsule uses X as the radius and Z as the half height INCLUDING the two
 * hemispheres (the Unreal convention, so a capsule whose half height equals its radius is a sphere); Box uses the three
 * half extents. Unused components are ignored. The capsule axis is the local Z axis. Local is applied before the carrier's
 * world transform, and its scale scales the extent (a sphere by the largest component, a capsule radius by the larger of X
 * and Y, its half height by Z, a box per axis), which is what the hitbox keys interpolate.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	EGSTN_ShapeType Type = EGSTN_ShapeType::Sphere;

	/** Radius, radius + half height, or half extents (see the struct comment), in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	FVector Extent = FVector(50.0, 50.0, 50.0);

	/** Placement relative to the carrier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shape")
	FTransform Local = FTransform::Identity;

	/** The smallest positive dimension in world units once the carrier's transform is applied; 0 if the shape is degenerate */
	double GetSmallestWorldExtent(const FTransform& CarrierWorld) const;
};
