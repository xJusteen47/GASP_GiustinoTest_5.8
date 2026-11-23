// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Placement/GSTN_LinkExitPlacer.h"
#include "GSTN_Placer_Ring.generated.h"

/**
 * The default exit (release, escape): the exiting entity lands on a ring around the partner. Candidates come in a fixed order: the
 * preferred direction first, then the seven others of an eight-direction ring turning counter-clockwise about the local up in 45
 * degree steps, then straight above. The first candidate whose capsule is free wins; if none is, the result is the fallback next to
 * the partner and reported invalid.
 */
UCLASS(meta = (DisplayName = "Ring Placer"))
class GSTNLINKCORE_API UGSTN_Placer_Ring : public UGSTN_LinkExitPlacer
{
	GENERATED_BODY()

public:
	/** Candidates of the ring in the placer's order: eight directions, then above */
	static constexpr int32 DirectionCount = 8;
	static constexpr int32 AboveIndex = DirectionCount;

	/** Distance from the partner's position to a ring candidate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RadiusCm = 120.f;

	/** Height of the ring candidates above the partner's position, along the local up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ForceUnits = "cm"))
	float HeightOffsetCm = 0.f;

	/** Space left over the capsule for the candidate above the partner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AboveClearanceCm = 20.f;

	/** Speed away from the partner given to a ring candidate; 0 = the entity leaves at rest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float OutwardSpeedCm = 0.f;

	/** Height of the fallback above the partner's position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ring", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FallbackLiftCm = 100.f;

	virtual FGSTN_ExitResult Place_Implementation(const FGSTN_ExitQuery& Query) const override;

	/**
	 * The unit direction of ring candidate Index (0 = preferred): the preferred direction projected on the plane of the local up, then
	 * turned Index x 45 degrees counter-clockwise about the up. A zero preferred direction means the partner's forward; a degenerate
	 * forward (along the up) falls back to the world X axis projected the same way
	 */
	static FVector RingDirection(const FGSTN_ExitQuery& Query, int32 Index);
};
