// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PlatformTypes.generated.h"

/**
 * State of one platform shape (traversal-surfaces.md 4.4, Platform.State.*). Derived from the shape's timeline at a frame, never
 * stored in a sync state.
 */
UENUM(BlueprintType)
enum class EGSTN_PlatformShapeState : uint8
{
	/** No collision */
	Inactive,
	/** Solid, posed with the enemy */
	Active,
	/** Solid, with the brace telegraph of a mount. Reserved: occupancy is T7, nothing produces it yet */
	Braced,
	/** Still solid, switches off at the end of its drop grace: the telegraph before a loss (a part break, the enemy launched) */
	Breaking
};

/**
 * What the enemy is doing, as far as its platform shapes care (many-enemies-replication.md 4.2 ActiveActionStates). The enemy's
 * combat state is mapped to it by whoever drives the platform (T7 wires the fighter's combat state); the platform plugin never
 * reads combat. Bit indices of the ActiveActionStates masks.
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "false"))
enum class EGSTN_PlatformActionState : uint8
{
	Idle,
	Moving,
	Attacking,
	Hitstun,
	Launched,
	Grabbed,
	Dying,
	Spawning
};

namespace GSTNPlatform
{
	/** Bit of an action state in an ActiveActionStates mask */
	inline constexpr uint8 ActionBit(EGSTN_PlatformActionState State) { return uint8(1u << uint8(State)); }

	/** The shipped default: a platform in every state but Launched, Grabbed, Dying and Spawning (many-enemies-replication.md 4.6) */
	inline constexpr uint8 DefaultActiveActionStates = ActionBit(EGSTN_PlatformActionState::Idle) | ActionBit(EGSTN_PlatformActionState::Moving)
		| ActionBit(EGSTN_PlatformActionState::Attacking) | ActionBit(EGSTN_PlatformActionState::Hitstun);

	/** True if the state has collision */
	inline constexpr bool IsSolid(EGSTN_PlatformShapeState State) { return State != EGSTN_PlatformShapeState::Inactive; }
}

/**
 * How an enemy responds to a character using one of its shapes (traversal-surfaces.md 4.3, owner TS.4 2026-10-22: "depends on the
 * part and the enemy"): an enemy-level default on the spec, overridable per shape. Data only in T3: the AI limiter, the brace and
 * OnOccupantChanged read it in T7. No response (the default) = the enemy moves at full speed.
 */
USTRUCT(BlueprintType)
struct GSTNPLATFORMCORE_API FGSTN_OccupantResponse
{
	GENERATED_BODY()

	/** A character on this shape counts as an occupant (the enemy may slow down, never despawns with it on) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occupant")
	bool bCountsAsOccupant = true;

	/** Speed limit of the enemy's AI while it has an occupant; 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occupant", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MaxSpeedWhileOccupied = 0.f;

	/** Optional telegraph the enemy plays when a character mounts (any asset the game's presentation knows how to play) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occupant")
	TSoftObjectPtr<UObject> BraceTimeline;

	/** The response applies to a character wall sliding or climbing on the shape */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occupant")
	bool bApplyToWallSlide = true;

	/** The response applies to a character standing on the shape */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occupant")
	bool bApplyToStand = true;
};
