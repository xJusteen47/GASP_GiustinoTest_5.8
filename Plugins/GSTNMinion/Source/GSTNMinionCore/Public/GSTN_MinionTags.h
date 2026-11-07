// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

enum class EGSTN_MinionActionState : uint8;
enum class EGSTN_MinionLODTier : uint8;

/**
 * Native tags of the minion plugin (combat-m8-minions.md 4c), all declared in this Core module so no other module defines a tag.
 *
 * Minions have no ability system component: no tag below is ever granted as a loose tag on a minion. "Who adds" means who
 * writes it into an event container, a hit victim view or a query result. The state and LOD tags are derived names of the
 * store's enums (for Blueprint and StateTree queries and debug); the wire carries the enum, never a tag. The designer-made
 * leaves (Minion.Type.<Name>, Command.Minion.<Name>) are ini only, in Config/Tags/GSTNMinion.ini; only their parent
 * Minion.Type is native.
 */
namespace GSTNMinionTags
{
	/** Marker of a minion in event containers and hit victim views (the fighter's twin is Combat.Entity.Fighter) */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Minion);

	/** Authoritative event: a minion was spawned. Published once by the spawner on the authority, never removed */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_MinionSpawned);
	/** Authoritative event: a minion was despawned (after death and grace time, or recycled) */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_MinionDespawned);
	/** Authoritative event: a minion became an elite or boss. Only a script asks for it, and it never goes back */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_MinionPromoted);

	/** Names of EGSTN_MinionActionState, in the same order as the enum */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Idle);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Walk);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Move);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hitstop);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hitstun);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Launched);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Knockdown);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_GetUp);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dying);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Spawning);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_PushedBack);

	/** Names of EGSTN_MinionLODTier (T0..T3) for StateTree conditions and LOD change callbacks */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LOD_Combat);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LOD_Near);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LOD_Far);
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LOD_Dormant);

	/** Root of the type tags: UGSTN_MinionDefinition::TypeTag must be a child of it. The leaves are ini */
	GSTNMINIONCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type);

	/** Appends every native tag above, once each (for the tags test and tools) */
	GSTNMINIONCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);

	/** The tag naming a state; an out-of-range value gives the invalid tag */
	GSTNMINIONCORE_API FGameplayTag GetActionStateTag(EGSTN_MinionActionState State);

	/** The tag naming a tier; an out-of-range value gives the invalid tag */
	GSTNMINIONCORE_API FGameplayTag GetLODTierTag(EGSTN_MinionLODTier Tier);
}
