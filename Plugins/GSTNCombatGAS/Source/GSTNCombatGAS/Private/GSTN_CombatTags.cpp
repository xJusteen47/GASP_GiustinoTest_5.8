// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "GSTN_CombatTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Timeline, "Combat.Ability.Timeline", "Identifies an ability that plays a combat timeline. Set in the ability's AbilityTags");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_AwaitingAdoption, "Combat.Ability.AwaitingAdoption", "A triggered ability waits to be adopted by the timeline that spawned it. Owned by that ability while it waits");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lock_Movement, "Combat.Lock.Movement", "Locks the combatant's movement. Granted by abilities and effects that root the combatant, read by the stats movement modifier");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lock_Reaction, "Combat.Lock.Reaction", "Locks the movement of a combatant in a hit reaction or KO. Projected by the combat tick from the sync state, never granted");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Role_Primary, "Combat.Weapon.Role.Primary", "Role of the weapon the combatant uses by default. Granted by the stats effect of the active weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Role_Support, "Combat.Weapon.Role.Support", "Role of the weapon the combatant can switch to. Granted by the stats effect of the active weapon");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_TimelineStarted, "Combat.Event.TimelineStarted", "A combat timeline started. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_TimelineFinished, "Combat.Event.TimelineFinished", "A combat timeline finished or ended. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_WeaponSwitched, "Combat.Event.WeaponSwitched", "The active weapon changed. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_LoadoutSwapped, "Combat.Event.LoadoutSwapped", "The combat simulation swapped to a new loadout table (the boundary frame). Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_SuperUsed, "Combat.Event.SuperUsed", "A super was used. Published once by the combat tick on the authority");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_HitLanded, "Combat.Event.HitLanded", "A hit landed. Outbox event on the attacker, published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_HitReceived, "Combat.Event.HitReceived", "A hit was received. Outbox event on the victim, published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_KO, "Combat.Event.KO", "A combatant was knocked out; the KOReason tag says why. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Received, "Combat.Event.Hit.Received", "Gameplay event sent to the victim's abilities when it is hit. The payload is in the event's effect context");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Impact, "Combat.Event.Impact", "Root of the surface impact events (splat, bounce, break). The leaves are declared by the surface impact checkpoint");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(KOReason_Health, "Combat.KOReason.Health", "KO reason: the health or the damage cap was reached. Named on the KO event only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(KOReason_Zone, "Combat.KOReason.Zone", "KO reason: the combatant entered a KO zone. Named on the KO event only");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Tumble, "Combat.State.Tumble", "The combatant was launched hard enough to tumble. Projected by the combat tick (M4), never granted");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cue_Hit, "GameplayCue.Combat.Hit", "Root of the hit cues. Leaf cues are designer content in a tag ini, code never names one");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Command, "Combat.Source.Command", "A command started the timeline. On the TimelineStarted event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Forced, "Combat.Source.Forced", "A forced command (test or scripted) started the timeline. On the TimelineStarted event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Adopted, "Combat.Source.Adopted", "A trigger, effect, other ability or AI started the ability and the combat tick adopted it. On the TimelineStarted event");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MeterCost, "Combat.SetByCaller.MeterCost", "Set-by-caller key: the meter cost an effect spends. Set by the activating ability");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "Combat.SetByCaller.Damage", "Set-by-caller key: the damage of a hit, in the unit of the damage mode. Read by the damage execution");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Meter, "Combat.SetByCaller.Meter", "Set-by-caller key: the meter a gain effect adds. Set by whoever applies the effect");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Stat, "Combat.Stat", "Root of stat names. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Meter, "Combat.Meter", "Root of meter names. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_SetByCallerStat, "Combat.SetByCaller.Stat", "Root of per-stat set-by-caller keys. Leaves are designer content in a tag ini, code never names one");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		GetDeclaredCoreTags(OutTags);
		OutTags.Append({
			Ability_Timeline, Ability_AwaitingAdoption,
			Lock_Movement, Lock_Reaction,
			Weapon_Role_Primary, Weapon_Role_Support,
			Event_TimelineStarted, Event_TimelineFinished, Event_WeaponSwitched, Event_LoadoutSwapped, Event_SuperUsed,
			Event_HitLanded, Event_HitReceived, Event_KO, Event_Hit_Received, Event_Impact,
			KOReason_Health, KOReason_Zone,
			State_Tumble,
			Cue_Hit,
			Source_Command, Source_Forced, Source_Adopted,
			SetByCaller_MeterCost, SetByCaller_Damage, SetByCaller_Meter,
			Root_Stat, Root_Meter, Root_SetByCallerStat });
	}
}
