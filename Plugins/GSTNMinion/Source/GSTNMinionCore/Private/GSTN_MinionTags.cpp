// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "GSTN_MinionTags.h"

#include "LOD/GSTN_MinionLOD.h"
#include "Store/GSTN_MinionStore.h"

namespace GSTNMinionTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Minion, "Combat.Entity.Minion", "Marker of a minion in event containers and hit victim views, the twin of Combat.Entity.Fighter. Written by the minion subsystem, never granted as a loose tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_MinionSpawned, "Combat.Event.MinionSpawned", "A minion was spawned. Authoritative event published once by the spawner on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_MinionDespawned, "Combat.Event.MinionDespawned", "A minion was despawned after its death grace time or recycled. Authoritative event published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_MinionPromoted, "Combat.Event.MinionPromoted", "A minion was promoted to an elite or a boss by a script. Authoritative event published once on the authority");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Idle, "Minion.State.Idle", "Minion action state Idle. Derived from the store for queries and debug, never stored as a tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Walk, "Minion.State.Walk", "Minion action state Walk. Derived from the store for queries and debug, never stored as a tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Move, "Minion.State.Move", "Minion action state Move, running or playing a move that travels. Derived from the store, never stored as a tag");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Hitstop, "Minion.State.Hitstop", "Minion action state Hitstop, frozen by a hit. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Hitstun, "Minion.State.Hitstun", "Minion action state Hitstun, the stagger in place reaction tier. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Launched, "Minion.State.Launched", "Minion action state Launched, the launch reaction tier. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Knockdown, "Minion.State.Knockdown", "Minion action state Knockdown, downed after a launch. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_GetUp, "Minion.State.GetUp", "Minion action state GetUp, standing back up. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dying, "Minion.State.Dying", "Minion action state Dying, the death timeline is playing. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "Minion.State.Dead", "Minion action state Dead, waiting for its grace time before despawn. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Spawning, "Minion.State.Spawning", "Minion action state Spawning, the spawn timeline is playing. Derived from the store for queries and debug");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_PushedBack, "Minion.State.PushedBack", "Minion action state PushedBack, the push back reaction tier. Derived from the store for queries and debug");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LOD_Combat, "Minion.LOD.Combat", "Level of detail tier T0, everything runs every frame. Derived by the batch LOD stage for StateTree conditions");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LOD_Near, "Minion.LOD.Near", "Level of detail tier T1, stepped every second frame. Derived by the batch LOD stage for StateTree conditions");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LOD_Far, "Minion.LOD.Far", "Level of detail tier T2, steered only and cannot be hit. Derived by the batch LOD stage for StateTree conditions");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LOD_Dormant, "Minion.LOD.Dormant", "Level of detail tier T3, no movement and no AI, only a wake check. Derived by the batch LOD stage");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Type, "Minion.Type", "Root of the minion type tags. UGSTN_MinionDefinition::TypeTag is a child of it, the leaves are designer-made in Config/Tags/GSTNMinion.ini");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Entity_Minion,
			Event_MinionSpawned, Event_MinionDespawned, Event_MinionPromoted,
			State_Idle, State_Walk, State_Move, State_Hitstop, State_Hitstun, State_Launched, State_Knockdown, State_GetUp,
			State_Dying, State_Dead, State_Spawning, State_PushedBack,
			LOD_Combat, LOD_Near, LOD_Far, LOD_Dormant,
			Type });
	}

	FGameplayTag GetActionStateTag(EGSTN_MinionActionState State)
	{
		switch (State)
		{
		case EGSTN_MinionActionState::Idle: return State_Idle;
		case EGSTN_MinionActionState::Walk: return State_Walk;
		case EGSTN_MinionActionState::Move: return State_Move;
		case EGSTN_MinionActionState::Hitstop: return State_Hitstop;
		case EGSTN_MinionActionState::Hitstun: return State_Hitstun;
		case EGSTN_MinionActionState::Launched: return State_Launched;
		case EGSTN_MinionActionState::Knockdown: return State_Knockdown;
		case EGSTN_MinionActionState::GetUp: return State_GetUp;
		case EGSTN_MinionActionState::Dying: return State_Dying;
		case EGSTN_MinionActionState::Dead: return State_Dead;
		case EGSTN_MinionActionState::Spawning: return State_Spawning;
		case EGSTN_MinionActionState::PushedBack: return State_PushedBack;
		default: return FGameplayTag();
		}
	}

	FGameplayTag GetLODTierTag(EGSTN_MinionLODTier Tier)
	{
		switch (Tier)
		{
		case EGSTN_MinionLODTier::Combat: return LOD_Combat;
		case EGSTN_MinionLODTier::Near: return LOD_Near;
		case EGSTN_MinionLODTier::Far: return LOD_Far;
		case EGSTN_MinionLODTier::Dormant: return LOD_Dormant;
		default: return FGameplayTag();
		}
	}
}
