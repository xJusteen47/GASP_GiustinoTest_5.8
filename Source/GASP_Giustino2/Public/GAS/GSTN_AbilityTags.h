// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "NativeGameplayTags.h"

/** Native gameplay tags shared by the project's gameplay abilities. */
namespace GSTNAbilityTags
{
	/** Asset tag of the jump ability */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Jump);

	/** Asset tag of the wall slide ability: the wall entry transition looks for it on the ability system */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_WallSlide);

	/** Owned while the wall slide ability is active (the character is on the wall) */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_WallSliding);

	/** Grant it (e.g. from an effect) to prevent grabbing walls */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_WallSlideBlocked);

	/** Grant it (e.g. from an effect) to prevent jumping */
	GASP_GIUSTINO2_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_JumpBlocked);
}
