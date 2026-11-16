// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_MinionSteering.generated.h"

/**
 * Steering as stateless math on the ground plane (M8 G8): the brain turns a target into the MoveDir and DesiredSpeed of an intent.
 * Nothing here knows a world, a store or a clock, so the native brain, the StateTree tasks and the tests share one implementation
 * and the same inputs give the same output on every machine.
 *
 * Directions are unit vectors (or zero); speeds are cm/s. Separation takes the neighbours as a span the caller fills (the brain
 * passes the minions within a radius), so the library never walks the store itself.
 */
UCLASS()
class GSTNMINIONCORE_API UGSTN_MinionSteeringUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Unit direction from From to To; zero when they are closer than 0.1 cm (no direction to speak of) */
	UFUNCTION(BlueprintPure, Category = "Minion|Steering")
	static FVector2D Seek(const FVector2D& From, const FVector2D& To);

	/**
	 * Seek that stops: inside StopRadius of To the result is zero (direction and speed), between StopRadius and StopRadius + SlowRadius
	 * the speed falls linearly to zero, beyond it the speed is MaxSpeed. The stop radius is what keeps a crowd from piling onto its target
	 */
	UFUNCTION(BlueprintPure, Category = "Minion|Steering")
	static void Arrive(const FVector2D& From, const FVector2D& To, float MaxSpeed, float StopRadius, float SlowRadius, FVector2D& OutDirection, float& OutSpeed);

	/**
	 * Push away from the neighbours closer than Radius: each contributes along the line from it to Position, weighted by how deep it
	 * is inside the radius (1 at the same spot, 0 at Radius). The sum is clamped to unit length. A neighbour at Position itself is
	 * skipped (no direction), so a minion never pushes itself away when the caller leaves it in the span
	 */
	static FVector2D Separation(const FVector2D& Position, TConstArrayView<FVector2D> Neighbours, float Radius);

	/** Seek direction blended with a separation push (weight 0 = pure seek); the result has unit length or is zero */
	UFUNCTION(BlueprintPure, Category = "Minion|Steering")
	static FVector2D BlendSeparation(const FVector2D& SeekDirection, const FVector2D& SeparationPush, float SeparationWeight);

	/** World yaw in degrees of a ground direction (0 = +X); 0 for a zero direction */
	UFUNCTION(BlueprintPure, Category = "Minion|Steering")
	static float YawOfDirection(const FVector2D& Direction);
};
