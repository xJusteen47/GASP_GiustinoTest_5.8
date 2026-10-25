// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformShape.h"
#include "GSTN_PlatformSpec.generated.h"

/**
 * What makes an enemy a platform (many-enemies-replication.md 4.2): its shapes and how they appear and go away. Fighters carry
 * it in a UGSTN_PlatformDefinition data asset or inline on UGSTN_EnemyPlatformComponent; platform minions will carry it in the
 * GSTNPlatformMinion bridge's extension (not built).
 *
 * Static configuration, identical on every machine.
 */
USTRUCT(BlueprintType)
struct GSTNPLATFORMCORE_API FGSTN_PlatformSpec
{
	GENERATED_BODY()

	/** The colliders, at most GSTNPlatform::MaxShapes, each with a distinct ShapeId */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (TitleProperty = "ShapeId"))
	TArray<FGSTN_PlatformShape> Shapes;

	/**
	 * Frames a shape stays solid in the Breaking state before it switches off, when it is taken away with a telegraph (a part
	 * break, the enemy launched): 20 frames are about 320 ms at 62.5 Hz. A shape may override it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (ClampMin = "0", ClampMax = "255"))
	int32 DropGraceFrames = 20;

	/** Index into Shapes of a shape id, INDEX_NONE if no shape has it */
	int32 FindShapeIndex(int32 ShapeId) const;

	/** Bit per shape id that exists in Shapes */
	uint8 GetShapeIdMask() const;

	/**
	 * Errors and warnings shared by IsDataValid and the tests (too many shapes, ids out of range or repeated, degenerate extents).
	 * True if there is no error
	 */
	bool CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;
};
