// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the Platform root (modularity-and-tags 4.2, owner GSTNPlatform; the bridges GSTNPlatformParts and
 * GSTNPlatformMinion declare none).
 *
 * Platform.State.* names the state of one platform shape (EGSTN_PlatformShapeState) for presentation, cues and designer queries.
 * The simulation never grants or stores these tags: the state is derived from the shape's timeline at the pose point, and
 * UGSTN_EnemyPlatformComponent::GetShapeStateTag maps it to a tag on demand. Platform.Event.* are local presentation triggers.
 */
namespace GSTNPlatformTags
{
	/** Root of every platform tag. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Platform);

	/** Root of the shape states. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_State);

	/** The shape is solid and posed with the enemy. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Active);

	/** The shape is solid and a brace telegraph plays (mount, T7). Reserved: nothing produces it yet. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Braced);

	/** The shape is still solid but will switch off at the end of its drop grace (the telegraph before a loss). */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Breaking);

	/** The shape has no collision. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Inactive);

	/** Root of the local presentation events. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Event);

	/** Local presentation trigger: a character started using a shape. Reserved: occupancy is T7. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Mounted);

	/** Local presentation trigger: a shape a character used went away. Reserved: occupancy is T7. */
	GSTNPLATFORMCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Dropped);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNPLATFORMCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
