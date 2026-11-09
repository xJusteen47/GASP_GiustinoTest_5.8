// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"

/**
 * Right-stick steering during a move's startup (plan precisione-weakpoint 5): the aim stick turns the move's direction, but never
 * more than the timeline's MaxSteerDegrees away from the facing the move started with. It sits on top of the aim policy (which picks
 * the start facing) and does not replace it.
 *
 * Pure function of the start yaw, the stick and the camera yaw of the frame's input command, so a client and the server compute
 * the same yaw from the same command and a re-simulation gives it again. The sim keeps the start yaw in its sync state and calls
 * this every startup frame: the steer is a clamp against the start, not an accumulated turn, so a dropped frame cannot drift it.
 */
struct GSTNCOMBATCORE_API FGSTN_StartupSteer
{
	/** Stick magnitude (unit circle) under which the aim stick does not steer: a resting thumb must not turn an attack */
	static constexpr float DeadZone = 0.25f;

	/**
	 * Yaw (degrees, -180..180) the move faces after steering. The stick (+Y up, as on the controller) is turned into a world yaw with
	 * the camera yaw (+Y = camera forward), and StartYawDeg is turned toward it by at most MaxSteerDegrees either way. Inside the dead
	 * zone, or with MaxSteerDegrees <= 0, it is StartYawDeg.
	 */
	static float SteerYaw(float StartYawDeg, const FVector2D& AimStick, float CameraYawDeg, float MaxSteerDegrees);
};
