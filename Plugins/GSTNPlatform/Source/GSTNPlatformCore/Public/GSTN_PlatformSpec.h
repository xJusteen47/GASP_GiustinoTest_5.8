// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformShape.h"
#include "GSTN_PlatformTypes.h"
#include "GSTN_PlatformSpec.generated.h"

/**
 * What validation needs to know beyond the spec: the wall modes' angle range, the walkable angle, whether part names can be
 * resolved. The defaults are the wall slide v2 range (wall-slide-v2.md 6.8) and "no part bridge"
 */
struct GSTNPLATFORMCORE_API FGSTN_PlatformValidationContext
{
	/** Lowest angle between a wall face's normal and the enemy's up that the wall modes accept (deg) */
	double MinWallSurfaceAngle = 45.0;
	/** Highest such angle (overhangs, deg) */
	double MaxWallSurfaceAngle = 120.0;
	/** Steepest top a character can stand on (deg, Mover's walkable angle) */
	double MaxStandAngle = 44.0;
	/** A part bridge resolves PartName (GSTNPlatformParts, T6): an unknown name is its error; without it a name is only a warning */
	bool bPartNamesResolved = false;
};

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

	/** Enemy action states in which the shapes are solid (EGSTN_PlatformActionState bits); a shape may override it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform", meta = (Bitmask, BitmaskEnum = "/Script/GSTNPlatformCore.EGSTN_PlatformActionState"))
	int32 ActiveActionStates = GSTNPlatform::DefaultActiveActionStates;

	/** How the enemy responds to a character on any of its shapes, unless the shape overrides it (data only until T7) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform")
	FGSTN_OccupantResponse DefaultOccupantResponse;

	/** Index into Shapes of a shape id, INDEX_NONE if no shape has it */
	int32 FindShapeIndex(int32 ShapeId) const;

	/** Bit per shape id that exists in Shapes */
	uint8 GetShapeIdMask() const;

	/** Drop grace of a shape: its own, else the spec's */
	int32 GetDropGraceFrames(const FGSTN_PlatformShape& Shape) const;

	/** Shapes (bit per id) that are solid in an action state */
	uint8 GetActionStateMask(EGSTN_PlatformActionState State) const;

	/** Occupant response of a shape: its own, else the spec's default */
	const FGSTN_OccupantResponse& GetOccupantResponse(const FGSTN_PlatformShape& Shape) const;

	/**
	 * Errors and warnings shared by IsDataValid and the tests (traversal-surfaces.md 4.1): too many shapes, ids out of range or
	 * repeated, degenerate extents, a shape on a bone, a Stand shape without a flat walkable top (a sphere or a capsule: its top fails
	 * the walkable test at the edges), a wall shape with no face in the wall modes' angle range ("players will never grab it"),
	 * invalid launch parameters, a part name nothing resolves. True if there is no error
	 */
	bool CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings,
		const FGSTN_PlatformValidationContext& Context = FGSTN_PlatformValidationContext()) const;
};
