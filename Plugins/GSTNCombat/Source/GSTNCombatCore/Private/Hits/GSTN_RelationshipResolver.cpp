// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_RelationshipResolver.h"

#include "Hits/GSTN_CombatHitTags.h"

FGameplayTag UGSTN_RelationshipResolver::Resolve_Implementation(const FGSTN_RelationshipQuery& Query) const
{
	return DefaultRule(Query);
}

FGameplayTag UGSTN_RelationshipResolver::DefaultRule(const FGSTN_RelationshipQuery& Query)
{
	if (Query.AttackerEntityId != INDEX_NONE && Query.AttackerEntityId == Query.VictimEntityId)
	{
		return GSTNCombatHitTags::Relationship_Self.GetTag();
	}
	// 255 is "no team": it is never an ally of anyone, not even of another entity with no team
	if (Query.AttackerTeamId == 255 || Query.VictimTeamId == 255)
	{
		return GSTNCombatHitTags::Relationship_Neutral.GetTag();
	}
	return Query.AttackerTeamId == Query.VictimTeamId ? GSTNCombatHitTags::Relationship_Ally.GetTag() : GSTNCombatHitTags::Relationship_Enemy.GetTag();
}
