// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the Surface root (modularity-and-tags 4.2, owner GSTNSurface, TS.1 2026-10-22).
 *
 * Surface.Behaviour.* says which traversal mechanics a collider offers. Consumers read them through FGameplayTagQuery fields
 * (the wall filter's RequiredBehaviours, a platform shape's collision rule), never by naming a leaf in their logic, so a game
 * can add its own behaviour leaves under this root. "No wall slide" is not a tag: the absence of WallSlide is the negative.
 *
 * Surface.Type.* leaves (Wall, Glass, Crate, Trampoline) are designer content in Config/Tags/GSTNSurface.ini; only the root is
 * native, so tag pickers can filter on it and code never names a leaf. No Surface tag is ever stored in a sync state: the
 * simulation keeps the uint8 index of the surface type table (UGSTN_SurfaceSettings::GetSurfaceTypeIndex).
 */
namespace GSTNSurfaceTags
{
	/** Root of every surface tag. */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Surface);

	/** Root of the impact classes of surfaces (H11). Leaves are ini content, code never names one. */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Type);

	/** Root of the traversal behaviours a surface offers. */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Behaviour);

	/** The surface can be wall slid on (the slide modes' default filter). */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_WallSlide);

	/** The surface can be wall climbed (the climb modes' default filter). */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_WallClimb);

	/** Landing on the surface launches a non-tumbling character with the surface's launch parameters. */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Launch);

	/** The surface is a walkable top a character can stand on (a platform's top). */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Stand);

	/** Reserved: a low-friction surface. Declared so content can be authored; no reader yet (TS.8). */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_Slippery);

	/** Reserved: a surface that blocks from one side only. Declared so content can be authored; no reader yet (TS.8). */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Behaviour_OneWay);

	/** Local presentation trigger raised when a surface launch is applied (never read by the simulation). */
	GSTNSURFACECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Launched);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNSURFACECORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
