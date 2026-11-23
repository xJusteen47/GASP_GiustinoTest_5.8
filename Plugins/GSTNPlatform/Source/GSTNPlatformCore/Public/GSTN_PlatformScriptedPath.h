// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformScriptedPath.generated.h"

/**
 * An authored motion for a platform enemy on rails (many-enemies-replication.md 1.5 approach C: "pose = f(server time, authored
 * path)"): a back-and-forth translation along an axis plus a back-and-forth yaw, both triangle waves of the simulation time. The
 * pose is never integrated from the previous frame, so a client predicting a frame, the server and a re-simulation all compute the
 * same pose for it. Used by the scripted test fighter (plan T3 scenario) and available to designers for rails enemies.
 */
USTRUCT(BlueprintType)
struct GSTNPLATFORMCORE_API FGSTN_PlatformScriptedPath
{
	GENERATED_BODY()

	/** Direction of the translation in the origin's frame (normalised on use) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path")
	FVector Axis = FVector(0.0, 1.0, 0.0);

	/** Speed of the translation (cm/s); 0 = no translation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float Speed = 300.f;

	/** Half the stroke: the enemy moves between -HalfLength and +HalfLength around the origin (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float HalfLength = 400.f;

	/** Yaw speed of the back-and-forth turn (deg/s); 0 = no turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float YawRate = 0.f;

	/** Half the turn (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Path", meta = (ClampMin = "0.0", ForceUnits = "deg"))
	float YawRange = 0.f;

	/** The pose at a time (s) for an origin (scale ignored). Pure */
	FTransform Evaluate(const FTransform& Origin, double TimeSeconds) const;

	/** Value of a triangle wave of the given slope (units/s) between -Amplitude and +Amplitude, starting at 0 and rising. Pure */
	static double TriangleWave(double TimeSeconds, double Slope, double Amplitude);
};
