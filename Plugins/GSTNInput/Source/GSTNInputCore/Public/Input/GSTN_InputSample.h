// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InputCoreTypes.h"

/**
 * The stick of one frame after classification: its direction in each of the three spaces, how far it is pushed and
 * whether it just flicked. Produced by FGSTN_DirectionClassifier; everything is small integers/enums so it is cheap to
 * copy into the host sync state.
 */
struct FGSTN_StickSample
{
	/** Direction of the physical stick (Up = pushed up on the controller) */
	EGSTN_StickDirection StickDir = EGSTN_StickDirection::Neutral;

	/** Direction relative to the character (Up = forward) */
	EGSTN_StickDirection FacingDir = EGSTN_StickDirection::Neutral;

	/** Direction in level axes (Up = the settings' world reference yaw) */
	EGSTN_StickDirection WorldDir = EGSTN_StickDirection::Neutral;

	/** How far the stick is pushed, the same in every space */
	EGSTN_StickZone Zone = EGSTN_StickZone::Neutral;

	/** True on the frame the stick enters Tilt fast enough from Neutral (see FGSTN_DirectionClassifier) */
	bool bFlick = false;

	/**
	 * World heading the stick points to (the stick rotated by the camera yaw), 256 steps per turn: what aims an attack.
	 * 0 when the stick is Neutral
	 */
	uint8 WorldYaw = 0;

	/** Direction in the given space */
	EGSTN_StickDirection GetDirection(EGSTN_DirectionSpace Space) const
	{
		switch (Space)
		{
		case EGSTN_DirectionSpace::Facing:
			return FacingDir;
		case EGSTN_DirectionSpace::World:
			return WorldDir;
		default:
			return StickDir;
		}
	}

	/** Same directions and zone (the aim heading and the flick flag are not part of a direction change) */
	bool HasSameDirections(const FGSTN_StickSample& Other) const
	{
		return StickDir == Other.StickDir && FacingDir == Other.FacingDir && WorldDir == Other.WorldDir && Zone == Other.Zone;
	}

	bool operator==(const FGSTN_StickSample& Other) const
	{
		return HasSameDirections(Other) && bFlick == Other.bFlick && WorldYaw == Other.WorldYaw;
	}
};

/**
 * What the host tick sees for one frame: the button masks and the classified stick. The history is fed with exactly
 * one sample per simulated frame. Built in the simulation from the input command and the direction classifier (M2), and
 * by FGSTN_InputScript in tests, the validator and debug tools.
 *
 * Pressed includes taps shorter than a frame (a button can be in Pressed and Released of the same frame and in neither
 * Held): the history records the press and then the release on that frame.
 */
struct FGSTN_InputSample
{
	int32 Frame = INDEX_NONE;

	/** Buttons held at the end of the frame (bit i = button i of the input config) */
	uint32 Held = 0;

	/** Buttons that went down during the frame, taps included */
	uint32 Pressed = 0;

	/** Buttons that went up during the frame */
	uint32 Released = 0;

	FGSTN_StickSample Stick;
};
