// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_AbilityTags.h"

namespace GSTNAbilityTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Jump, "Ability.Movement.Jump", "Jump ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_WallSlide, "Ability.Movement.WallSlide", "Wall slide ability, activated by the wall entry transition");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_WallSliding, "State.Movement.WallSliding", "Owned while the wall slide ability is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_WallSlideBlocked, "State.Movement.WallSlideBlocked", "Blocks the wall slide ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_JumpBlocked, "State.Movement.JumpBlocked", "Blocks the jump ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AnimSet_Wall, "Anim.Set.Wall", "Anim set slot of the wall modes' layer (wall slide and climb v2)");
}
