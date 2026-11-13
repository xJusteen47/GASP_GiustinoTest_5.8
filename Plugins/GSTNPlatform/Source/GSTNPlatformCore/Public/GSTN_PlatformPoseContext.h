// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformPoseContext.generated.h"

/**
 * One call of the pose point: which simulation frame's end pose the platform colliders take, and the time of that end
 * (many-enemies-replication.md 4.3, wall-slide-v2.md 6.14).
 *
 * The pose point runs before the movement of simulation frame F and poses the colliders at the end of frame F-1: Frame = F-1,
 * TimeMs = the start of F in server simulation timespace. A client predicting F, the server simulating F and a client
 * re-simulating F after a correction all get the same Frame and TimeMs, so a pose computed from them is the same on every
 * machine (the riding code of the wall modes needs the base pose of the previous frame to be identical everywhere).
 */
USTRUCT(BlueprintType)
struct GSTNPLATFORMCORE_API FGSTN_PlatformPoseContext
{
	GENERATED_BODY()

	/** Simulation frame whose end pose is applied; INDEX_NONE for a presentation pose (nothing simulates against it) */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	int32 Frame = INDEX_NONE;

	/** End of Frame in server simulation timespace (ms). For a presentation pose: the best estimate of the server's time */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	double TimeMs = 0.0;

	/** Length of a simulation frame (ms) */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	float StepMs = 0.f;

	/** True when a simulation step drives the pose point; false for the presentation fallback (no simulation running) */
	UPROPERTY(BlueprintReadOnly, Category = "Platform")
	bool bFromSimulation = false;

	double GetTimeSeconds() const { return TimeMs * 0.001; }
};
