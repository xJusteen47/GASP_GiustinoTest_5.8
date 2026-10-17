// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CombatCoreTypes.h"

/**
 * The stick of one frame after classification: its direction in each of the three spaces, how far it is pushed and
 * whether it just flicked. Produced by FGSTN_DirectionClassifier; everything is small integers/enums so it is cheap to
 * copy into the combat sync state.
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

	bool operator==(const FGSTN_StickSample& Other) const
	{
		return StickDir == Other.StickDir && FacingDir == Other.FacingDir && WorldDir == Other.WorldDir
			&& Zone == Other.Zone && bFlick == Other.bFlick && WorldYaw == Other.WorldYaw;
	}
};
