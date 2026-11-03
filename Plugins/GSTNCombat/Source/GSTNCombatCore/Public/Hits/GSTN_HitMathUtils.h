// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Hits/GSTN_HitData.h"
#include "GSTN_HitMathUtils.generated.h"

/** Directional influence tuning (combat-m4-hits 3, DI). Lives on the combat rules; M6 adds the strength curve and the victim scales */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_DIParams
{
	GENERATED_BODY()

	/** Yaw the full lateral stick turns the launch by */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float MaxDIYawDegrees = 15.f;

	/** Change of the launch's elevation dot (launch . up) the full stick along the launch causes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxDIElevationDot = 0.2f;

	/** A launch with elevation dot above this counts as upward, below as downward (slightly downward launches behave as upward ones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float UpwardThresholdDot = -0.1f;

	/** The elevation dot never exceeds this in magnitude, so the launch keeps a horizontal direction for the yaw */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxElevationDot = 0.98f;
};

/** The angles of the Sakurai3D special angle: low on a grounded victim below a knockback threshold, higher otherwise */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SakuraiParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sakurai", meta = (ClampMin = "-90.0", ClampMax = "90.0", ForceUnits = "deg"))
	float GroundedLowAngleDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sakurai", meta = (ClampMin = "-90.0", ClampMax = "90.0", ForceUnits = "deg"))
	float HighAngleDeg = 44.f;

	/** Below this knockback a grounded victim gets the low angle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sakurai", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float KnockbackThreshold = 800.f;
};

/**
 * Stateless launch direction and directional influence. Every function takes the victim's local up as an input (minus its
 * gravity direction at the frame the caller names); none reads world Z, a world or a component. Doubles throughout, so the same
 * inputs give the same bits on a machine; Sin / Cos come from the engine's FMath.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_HitMathUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** V with its component along Up removed (Up need not be unit length). Returns V when Up is zero */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static FVector ProjectOnPlane(const FVector& V, const FVector& Up);

	/**
	 * The unit launch direction: the yaw from YawSource on the plane normal to Up, the pitch from the angle above that plane.
	 * A yaw source with no horizontal part falls back to the attacker's facing, then to the direction to the victim, then to a
	 * fixed direction on the plane. Special angles: Sakurai3D replaces AngleDeg by the low angle (grounded victim, knockback below
	 * the threshold) or the high one; AutoLink is the hitbox velocity's own direction (the yaw source and angle when it has none).
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static FVector LaunchDirection(EGSTN_YawSource YawSource, float AngleDeg, EGSTN_SpecialAngle SpecialAngle, const FVector& Up, const FVector& AttackerFacing, const FVector& HitboxToVictim, const FVector& HitboxVelocity, bool bVictimGrounded, float Knockback, const FGSTN_SakuraiParams& Sakurai);

	/**
	 * Directional influence on a launch, steps 1 to 8 of combat-m4-hits 3: the stick (on the plane normal to Up) turns the launch
	 * about Up and changes its elevation in dot space; the speed is unchanged. DI in (stick along the launch) steepens the
	 * launch, DI out flattens it and never crosses the horizontal. YawSourceDirection is the horizontal fallback for a launch
	 * straight along Up. A stick of zero length returns the launch.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static FVector ApplyDI(const FVector& Launch, const FVector& StickOnPlane, const FVector& Up, const FVector& YawSourceDirection, const FGSTN_DIParams& Params);
};
