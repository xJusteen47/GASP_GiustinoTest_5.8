// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_WallEdgeTypes.generated.h"

class UPrimitiveComponent;

/**
 * What the look-ahead found on the wall in front of the character (wall-slide-v2.md 2.2 step 5).
 * Bump wins over the angle classes: a feature within step height is continuous surface, never a corner.
 */
UENUM(BlueprintType)
enum class EGSTN_WallEdgeKind : uint8
{
	/** Flat continuation, nothing to react to */
	None,
	/** Ledge or rib no taller than MaxStepHeight: stepped over, never a corner and never a floor */
	Bump,
	/** Gentle change of plane (angle <= MaxSmoothAngle): the proxy plane follows, no speed cap */
	Slope,
	/** Inside corner ahead: the surface rises toward the character */
	Concave,
	/** Outside edge ahead: the wall ends or turns away */
	Convex,
	/** Angle beyond the limits: a wall that cannot be turned onto */
	Blocked,
};

/**
 * Tunables of the edge look-ahead and of the corner speed cap (wall-slide-v2.md 2.3). Read by the pure functions of
 * UGSTN_WallEdgeUtils; the traces themselves come with the v2 mode.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallEdgeSettings
{
	GENERATED_BODY()

	/** Slope changes up to this angle are continuous: the plane follows them and nothing is capped (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ClampMax = "89", ForceUnits = "deg"))
	float MaxSmoothAngle = 15.f;

	/** Bumps and ledges up to this height are stepped over (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxStepHeight = 10.f;

	/** Extra clearance added to the step lift (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StepMargin = 2.f;

	/** Look-ahead distance is max(MinLookahead, speed * LookaheadTime) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ForceUnits = "s"))
	float LookaheadTime = 0.15f;

	/** Lower bound of the look-ahead distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinLookahead = 20.f;

	/** How deep the ahead trace looks for a receding surface (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RecessDepth = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges")
	bool bSlowOnConcave = true;

	/** Speed scale (of the terminal speed) at a 90 degree inside corner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bSlowOnConcave"))
	float ConcaveMinScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges")
	bool bSlowOnConvex = true;

	/** Speed scale (of the terminal speed) at a 90 degree outside edge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bSlowOnConvex"))
	float ConvexMinScale = 0.4f;

	/** Approach deceleration that shapes the cap curve before a corner (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float CornerBraking = 2000.f;

	/** The ahead traces ignore pawns (other characters are not wall geometry) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges")
	bool bIgnorePawns = true;
};

/** Result of the look-ahead (not reflected: filled by the traces, consumed by the tick). Angles in degrees. */
struct FGSTN_WallAheadResult
{
	EGSTN_WallEdgeKind Kind = EGSTN_WallEdgeKind::None;
	/** Angle between the wall normal and the surface found ahead (deg) */
	float Angle = 0.f;
	/** Distance of the surface relative to the wall plane: negative = bump of height -Offset, positive = recess (cm) */
	float Offset = 0.f;
	/** Distance from the character to the edge along the movement direction (cm) */
	float Distance = 0.f;
	FVector Normal = FVector::ZeroVector;
	FVector Point = FVector::ZeroVector;
	TWeakObjectPtr<UPrimitiveComponent> Component;
};
