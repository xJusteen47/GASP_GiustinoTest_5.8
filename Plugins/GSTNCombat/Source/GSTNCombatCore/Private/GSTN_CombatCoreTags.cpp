// Pure combat logic (no GAS, no Network Prediction).

#include "GSTN_CombatCoreTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Busy, "Combat.State.Busy", "The combatant is committed to an action and cannot start a new one freely. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_InTimeline, "Combat.State.InTimeline", "A combat timeline is playing. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SuperInput, "Combat.State.SuperInput", "The super input sequence is being entered. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CancelWindow, "Combat.State.CancelWindow", "The active timeline is inside a cancel window. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_OverrideWindow, "Combat.State.OverrideWindow", "The active timeline is inside its startup override window. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Falling, "Combat.State.Falling", "The combatant is falling relative to its local gravity. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Hitstun, "Combat.State.Hitstun", "The combatant is in hitstun. Projected by the combat tick (M4), never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Hitstop, "Combat.State.Hitstop", "The combatant is frozen by hitstop. Projected by the combat tick (M4), never granted");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Weapon, "Weapon", "Root of weapon identity tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Kit, "Kit", "Root of kit identity tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Super, "Super", "Root of super identity tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Loadout, "Loadout", "Root of loadout tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Skin, "Skin", "Root of skin tags. Leaves are designer content in a tag ini, code never names one");

	void GetDeclaredCoreTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			State_Busy, State_InTimeline, State_SuperInput, State_CancelWindow, State_OverrideWindow, State_Falling, State_Hitstun, State_Hitstop,
			Root_Weapon, Root_Kit, Root_Super, Root_Loadout, Root_Skin });
	}
}
