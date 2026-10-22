// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "GSTN_CombatTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Busy, "Combat.State.Busy", "The combatant is committed to an action and cannot start a new one freely. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_InTimeline, "Combat.State.InTimeline", "A combat timeline is playing. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SuperInput, "Combat.State.SuperInput", "The super input sequence is being entered. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_CancelWindow, "Combat.State.CancelWindow", "The active timeline is inside a cancel window. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_OverrideWindow, "Combat.State.OverrideWindow", "The active timeline is inside its startup override window. Projected by the combat tick, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Falling, "Combat.State.Falling", "The combatant is falling relative to its local gravity. Projected by the combat tick, never granted");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Timeline, "Combat.Ability.Timeline", "Identifies an ability that plays a combat timeline. Set in the ability's AbilityTags");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_AwaitingAdoption, "Combat.Ability.AwaitingAdoption", "A triggered ability waits to be adopted by the timeline that spawned it. Owned by that ability while it waits");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lock_Movement, "Combat.Lock.Movement", "Locks the combatant's movement. Granted by abilities and effects that root the combatant, read by the stats movement modifier");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Role_Primary, "Combat.Weapon.Role.Primary", "Role of the weapon the combatant uses by default. Granted by the stats effect of the active weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Role_Support, "Combat.Weapon.Role.Support", "Role of the weapon the combatant can switch to. Granted by the stats effect of the active weapon");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_TimelineStarted, "Combat.Event.TimelineStarted", "A combat timeline started. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_TimelineFinished, "Combat.Event.TimelineFinished", "A combat timeline finished or ended. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_WeaponSwitched, "Combat.Event.WeaponSwitched", "The active weapon changed. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_SuperUsed, "Combat.Event.SuperUsed", "A super was used. Published once by the combat tick on the authority");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MeterCost, "Combat.SetByCaller.MeterCost", "Set-by-caller key: the meter cost an effect spends. Set by the activating ability");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Weapon, "Weapon", "Root of weapon identity tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Kit, "Kit", "Root of kit identity tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Super, "Super", "Root of super identity tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Loadout, "Loadout", "Root of loadout tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Skin, "Skin", "Root of skin tags. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Stat, "Combat.Stat", "Root of stat names. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Meter, "Combat.Meter", "Root of meter names. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_SetByCallerStat, "Combat.SetByCaller.Stat", "Root of per-stat set-by-caller keys. Leaves are designer content in a tag ini, code never names one");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			State_Busy, State_InTimeline, State_SuperInput, State_CancelWindow, State_OverrideWindow, State_Falling,
			Ability_Timeline, Ability_AwaitingAdoption,
			Lock_Movement,
			Weapon_Role_Primary, Weapon_Role_Support,
			Event_TimelineStarted, Event_TimelineFinished, Event_WeaponSwitched, Event_SuperUsed,
			SetByCaller_MeterCost,
			Root_Weapon, Root_Kit, Root_Super, Root_Loadout, Root_Skin, Root_Stat, Root_Meter, Root_SetByCallerStat });
	}
}
