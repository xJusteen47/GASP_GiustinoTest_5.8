// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_CombatHitTags.h"

namespace GSTNCombatHitTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Hurtbox, "Combat.Hurtbox", "Root of the hurtbox classification tags. Leaves (Feet, Head, Torso) are designer content in the plugin tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Normal, "Combat.HitType.Normal", "A hit that damages and moves its victim. An unset hit type means the same");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Grab, "Combat.HitType.Grab", "A grab. Data only until a grab path exists");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Projectile, "Combat.HitType.Projectile", "A projectile hit. Data only until projectiles exist");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Unblockable, "Combat.HitType.Unblockable", "A hit that blocks and parries do not stop. Data only until shields exist");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Relationship_Self, "Combat.Relationship.Self", "The victim is the attacker itself. Key of the relationship variants of a hitbox");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Relationship_Ally, "Combat.Relationship.Ally", "The victim is on the attacker's side. Key of the relationship variants of a hitbox");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Relationship_Enemy, "Combat.Relationship.Enemy", "The victim is on an opposing side. Key of the relationship variants of a hitbox");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Relationship_Neutral, "Combat.Relationship.Neutral", "The victim belongs to no side. Key of the relationship variants of a hitbox");

	void GetDeclaredHitTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({ Root_Hurtbox, HitType_Normal, HitType_Grab, HitType_Projectile, HitType_Unblockable,
			Relationship_Self, Relationship_Ally, Relationship_Enemy, Relationship_Neutral });
	}
}
