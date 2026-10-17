// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_CombatCoreTypes.generated.h"

/**
 * One of the eight directions of a stick, or Neutral. What "Up" means depends on the space the stick was classified in
 * (EGSTN_DirectionSpace): in Facing space Up is forward of the character and Down is back, in Stick space Up is the
 * stick pushed up (away from the camera), in World space Up is the settings' world reference yaw.
 *
 * Declared as bit flags so a set of directions is a plain bit mask (1 << Direction), editable as such in the details
 * panel and cheap to test in the matcher. See FGSTN_DirectionMask.
 */
UENUM(BlueprintType, meta = (Bitflags))
enum class EGSTN_StickDirection : uint8
{
	Neutral,
	Up,
	UpRight,
	Right,
	DownRight,
	Down,
	DownLeft,
	Left,
	UpLeft,
};

/** Number of values of EGSTN_StickDirection */
inline constexpr int32 GSTN_NumStickDirections = 9;

/** The frame of reference a stick direction is expressed in (combat.md section 6.3). */
UENUM(BlueprintType)
enum class EGSTN_DirectionSpace : uint8
{
	/** Relative to the character: Up = forward, Down = back. For "towards / away from where I look" */
	Facing,

	/**
	 * The physical stick on the controller, independent of character and camera. For spins, codes and any input that
	 * must not change when the character or the camera turns
	 */
	Stick,

	/** Absolute level axes: Up = the world reference yaw of the settings. For fixed-camera or side-view sections */
	World,
};

/**
 * How far the stick is pushed. The same in every direction space. Ordered, so "at least Tilt" is a plain comparison.
 */
UENUM(BlueprintType)
enum class EGSTN_StickZone : uint8
{
	/** Inside the dead zone: no direction */
	Neutral,

	/** A light tilt: aims a "standing" attack without making it directional */
	Aim,

	/** A real direction */
	Tilt,
};

/** When the directions of a command are checked relative to its terminal press. */
UENUM(BlueprintType)
enum class EGSTN_DirectionPolicy : uint8
{
	/** At the frame of the press (what the player did when pressing) */
	AtPress,

	/** At the frame the command comes out (what the player holds when the move can start) */
	AtExecution,
};

/** Per-slot override of a default taken from the settings. */
UENUM(BlueprintType)
enum class EGSTN_OverrideBool : uint8
{
	/** Use the settings' value */
	Default,
	On,
	Off,
};

/** Which buffered presses compete when several commands match at once (combat.md section 8.2). */
UENUM(BlueprintType)
enum class EGSTN_BufferConflictPolicy : uint8
{
	/** Only the group of presses around the newest press competes: a player who changed their mind gets the new input */
	MostRecent,

	/** Every buffered press competes, only the command priority decides */
	HighestPriority,

	/** Only the group around the oldest buffered press competes (queue-like) */
	Oldest,
};

/**
 * A set of EGSTN_StickDirection values, bit (1 << Direction). Stored as uint16 in frame logic; the editor-facing
 * properties use int32 with the Bitmask metadata over EGSTN_StickDirection.
 */
using FGSTN_DirectionMask = uint16;

/** Constants and helpers over FGSTN_DirectionMask. */
namespace GSTN_DirectionMasks
{
	constexpr FGSTN_DirectionMask FromDirection(EGSTN_StickDirection Direction)
	{
		return static_cast<FGSTN_DirectionMask>(1u << static_cast<uint8>(Direction));
	}

	constexpr bool Contains(FGSTN_DirectionMask Mask, EGSTN_StickDirection Direction)
	{
		return (Mask & FromDirection(Direction)) != 0;
	}

	inline constexpr FGSTN_DirectionMask None = 0;
	inline constexpr FGSTN_DirectionMask Neutral = FromDirection(EGSTN_StickDirection::Neutral);

	/** The eight non-neutral directions */
	inline constexpr FGSTN_DirectionMask AnyTilted = 0x1FE;

	/** Every direction including Neutral */
	inline constexpr FGSTN_DirectionMask All = 0x1FF;

	/** In Facing space: the three directions that point ahead of the character */
	inline constexpr FGSTN_DirectionMask FacingForward = FromDirection(EGSTN_StickDirection::Up)
		| FromDirection(EGSTN_StickDirection::UpLeft) | FromDirection(EGSTN_StickDirection::UpRight);

	/** In Facing space: the three directions that point behind the character */
	inline constexpr FGSTN_DirectionMask FacingBack = FromDirection(EGSTN_StickDirection::Down)
		| FromDirection(EGSTN_StickDirection::DownLeft) | FromDirection(EGSTN_StickDirection::DownRight);
}
