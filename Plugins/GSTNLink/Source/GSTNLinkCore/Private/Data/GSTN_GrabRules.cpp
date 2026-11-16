// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_GrabRules.h"

#include "GSTN_CombatCoreTags.h"
#include "GSTN_LinkTags.h"

FGSTN_GrabRules::FGSTN_GrabRules()
{
	// Only tags this plugin or the combat core declare are named here. The state tags of the optional plugins are by name in
	// IgnoreTagNames, resolved at decision time, because a constructor may run before another plugin registered its tags
	Requirements.IgnoreTags.AddTag(GSTNLinkTags::Linked);
	Requirements.IgnoreTags.AddTag(GSTNLinkTags::Immunity_Grab);
	Requirements.IgnoreTags.AddTag(GSTNCombatTags::State_Hitstun);

	IgnoreTagNames = {
		FName(TEXT("Combat.State.Tumble")),
		FName(TEXT("Combat.State.Knockdown")),
		FName(TEXT("Combat.State.ShieldBroken")),
		FName(TEXT("Combat.State.Intangible")) };
}

FGSTN_LatchRules::FGSTN_LatchRules()
{
	Requirements.IgnoreTags.AddTag(GSTNLinkTags::Linked);
	Requirements.IgnoreTags.AddTag(GSTNLinkTags::Immunity_Latch);
}
