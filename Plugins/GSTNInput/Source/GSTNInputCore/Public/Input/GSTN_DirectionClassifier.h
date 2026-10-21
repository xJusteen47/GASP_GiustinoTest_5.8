// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_InputSample.h"

class UGSTN_InputSettings;

/**
 * The little memory the classifier needs between frames. It lives in the host sync state, so a flick is detected the
 * same way after a rollback.
 */
struct FGSTN_ClassifierState
{
	/** Zone of the previous frame */
	EGSTN_StickZone Zone = EGSTN_StickZone::Neutral;

	/** Last frame the stick was Neutral (INDEX_NONE if never): a flick is a quick leave of Neutral into Tilt */
	int32 LastNeutralFrame = INDEX_NONE;

	bool operator==(const FGSTN_ClassifierState& Other) const
	{
		return Zone == Other.Zone && LastNeutralFrame == Other.LastNeutralFrame;
	}
};

/**
 * Turns the raw stick of one frame into a FGSTN_StickSample: the direction in three spaces (combat.md section 6.3), the
 * zone and the flick flag.
 *
 * Pure and stateless apart from the FGSTN_ClassifierState passed in and out, so the simulation can call it at any frame
 * after a rollback. It is the only place that touches floating point in the input path: the command carries the stick
 * and the camera yaw quantized (int8, uint8) and callers must dequantize with the helpers below before classifying, so
 * the server and every client classify exactly the same numbers. Everything downstream (history, matcher) works on the
 * resulting enums and integers.
 *
 * Angles are degrees, 0 = up, clockwise. The world heading of the stick is the stick angle plus the camera yaw (Unreal
 * yaw is clockwise seen from above, like the stick angle); Facing space subtracts the character's yaw, World space the
 * settings' reference yaw. The zone depends only on the magnitude, so it is the same in every space.
 */
struct GSTNINPUTCORE_API FGSTN_DirectionClassifier
{
	/**
	 * Classifies one frame.
	 * @param RawStick Stick as on the controller (+Y up), each axis in [-1, 1].
	 * @param CameraYawDeg Control rotation yaw.
	 * @param FacingYawDeg Yaw the character faces (from the simulated Mover state).
	 * @param Prev State returned for the previous frame (default state on the first frame).
	 * @param Frame Current simulation frame.
	 * @param Settings Thresholds.
	 * @param OutState State to feed the next frame with.
	 */
	static FGSTN_StickSample Classify(const FVector2D& RawStick, float CameraYawDeg, float FacingYawDeg,
		const FGSTN_ClassifierState& Prev, int32 Frame, const UGSTN_InputSettings& Settings, FGSTN_ClassifierState& OutState);

	/** Zone of a stick magnitude: below the dead zone Neutral, below the aim zone Aim, else Tilt */
	static EGSTN_StickZone ClassifyZone(float Magnitude, const UGSTN_InputSettings& Settings);

	/**
	 * Sector of an angle (0 = up, clockwise, any range). Eight sectors, the diagonals extending DiagonalHalfAngle degrees
	 * on each side of their 45 degree line; a boundary angle belongs to the cardinal direction.
	 */
	static EGSTN_StickDirection AngleToDirection(float AngleDeg, float DiagonalHalfAngle);

	/** Angle of a stick vector (+Y up): 0 = up, 90 = right. 0 for a zero vector */
	static float GetStickAngle(const FVector2D& RawStick);

	/** Command stick byte -> [-1, 1] (127 = 1) */
	static float DequantizeStickAxis(int8 Value);

	/** [-1, 1] -> command stick byte, rounded, clamped to the unit square */
	static int8 QuantizeStickAxis(float Value);

	/** Command camera yaw byte -> degrees [0, 360), 256 steps per turn */
	static float DequantizeYaw(uint8 Value);

	/** Any yaw in degrees -> command camera yaw byte, rounded to the nearest step */
	static uint8 QuantizeYaw(float YawDeg);
};
