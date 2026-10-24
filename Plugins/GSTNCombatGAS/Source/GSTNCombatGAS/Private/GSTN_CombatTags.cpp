// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "GSTN_CombatTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Timeline, "Combat.Ability.Timeline", "Identifies an ability that plays a combat timeline. Set in the ability's AbilityTags");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_AwaitingAdoption, "Combat.Ability.AwaitingAdoption", "A triggered ability waits to be adopted by the timeline that spawned it. Owned by that ability while it waits");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lock_Movement, "Combat.Lock.Movement", "Locks the combatant's movement. Granted by abilities and effects that root the combatant, read by the stats movement modifier");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Role_Primary, "Combat.Weapon.Role.Primary", "Role of the weapon the combatant uses by default. Granted by the stats effect of the active weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Role_Support, "Combat.Weapon.Role.Support", "Role of the weapon the combatant can switch to. Granted by the stats effect of the active weapon");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_TimelineStarted, "Combat.Event.TimelineStarted", "A combat timeline started. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_TimelineFinished, "Combat.Event.TimelineFinished", "A combat timeline finished or ended. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_WeaponSwitched, "Combat.Event.WeaponSwitched", "The active weapon changed. Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_LoadoutSwapped, "Combat.Event.LoadoutSwapped", "The combat simulation swapped to a new loadout table (the boundary frame). Published once by the combat tick on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_SuperUsed, "Combat.Event.SuperUsed", "A super was used. Published once by the combat tick on the authority");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Command, "Combat.Source.Command", "A command started the timeline. On the TimelineStarted event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Forced, "Combat.Source.Forced", "A forced command (test or scripted) started the timeline. On the TimelineStarted event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_Adopted, "Combat.Source.Adopted", "A trigger, effect, other ability or AI started the ability and the combat tick adopted it. On the TimelineStarted event");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hurtbox_Head, "Combat.Hurtbox.Head", "Zone of a hurtbox on the head. Classifies a hurtbox (hurtbox set data, editor Physics Asset mapping); never granted to an actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hurtbox_Torso, "Combat.Hurtbox.Torso", "Zone of a hurtbox on the torso. Classifies a hurtbox; never granted to an actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hurtbox_Arm, "Combat.Hurtbox.Arm", "Zone of a hurtbox on an arm; the side is the bone name, not a tag. Classifies a hurtbox; never granted to an actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hurtbox_Leg, "Combat.Hurtbox.Leg", "Zone of a hurtbox on a leg; the side is the bone name, not a tag. Classifies a hurtbox; never granted to an actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hurtbox_Hand, "Combat.Hurtbox.Hand", "Zone of a hurtbox on a hand. Classifies a hurtbox; never granted to an actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hurtbox_Foot, "Combat.Hurtbox.Foot", "Zone of a hurtbox on a foot. Classifies a hurtbox; never granted to an actor");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MeterCost, "Combat.SetByCaller.MeterCost", "Set-by-caller key: the meter cost an effect spends. Set by the activating ability");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Stat, "Combat.Stat", "Root of stat names. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Meter, "Combat.Meter", "Root of meter names. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_SetByCallerStat, "Combat.SetByCaller.Stat", "Root of per-stat set-by-caller keys. Leaves are designer content in a tag ini, code never names one");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		GetDeclaredCoreTags(OutTags);
		OutTags.Append({
			Ability_Timeline, Ability_AwaitingAdoption,
			Lock_Movement,
			Weapon_Role_Primary, Weapon_Role_Support,
			Event_TimelineStarted, Event_TimelineFinished, Event_WeaponSwitched, Event_LoadoutSwapped, Event_SuperUsed,
			Source_Command, Source_Forced, Source_Adopted,
			Hurtbox_Head, Hurtbox_Torso, Hurtbox_Arm, Hurtbox_Leg, Hurtbox_Hand, Hurtbox_Foot,
			SetByCaller_MeterCost,
			Root_Stat, Root_Meter, Root_SetByCallerStat });
	}
}
