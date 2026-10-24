// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Hit vocabulary the core owns because code switches on it (combat-m4-hits 3). The hurtbox leaves (Combat.Hurtbox.Feet,
 * Head, Torso) are designer content in Config/Tags/GSTNCombat.ini and are never named in C++: only the root is declared so
 * tag pickers can filter on it. No tag is ever stored in a sync state or a snapshot (indices only).
 */
namespace GSTNCombatHitTags
{
	/** Root of the hurtbox classification tags. Leaves are ini content, code never names one */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Hurtbox);

	/** A hit that damages and moves its victim. An unset hit type means the same */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Normal);

	/** A grab. Data only until a grab path exists */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Grab);

	/** A projectile hit. Data only until projectiles exist */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Projectile);

	/** A hit that blocks and parries do not stop. Data only until shields exist */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Unblockable);

	/** The victim is the attacker itself (key of FGSTN_HitboxSpec::RelationshipVariants) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Relationship_Self);

	/** The victim is on the attacker's side */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Relationship_Ally);

	/** The victim is on an opposing side */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Relationship_Enemy);

	/** The victim belongs to no side */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Relationship_Neutral);

	/** Every tag declared here, for tests */
	GSTNCOMBATCORE_API void GetDeclaredHitTags(TArray<FGameplayTag>& OutTags);
}
