// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_DefenceTags.h"

namespace GSTNDefenceTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Shielding, "Combat.State.Shielding", "Shield bubble raised, from the end of startup. Projected every frame by the defence tick extension, never granted; never projected where the mode disables the shield");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ShieldStun, "Combat.State.ShieldStun", "Stalled by a blocked hit: the bubble stays up but no out-of-shield action is allowed. Projected until ShieldStunEndFrame by the defence tick extension");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ShieldBroken, "Combat.State.ShieldBroken", "Long stun after a shield break, launched upward and without a bubble. Projected until BreakEndFrame by the defence tick extension");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ParryWindow, "Combat.State.ParryWindow", "A parry window is open: the shield's perfect-shield window or a move's parry window track. Projected for the window, readable by UI and AI");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ParryRecovery, "Combat.State.ParryRecovery", "Defender's short recovery after a shield-source parry; a move keeps playing. Projected by the defence tick extension");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Parried, "Combat.State.Parried", "Attacker punished by a parry. Projected until PunishEndFrame by the defence tick extension from the punish inbox entry");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Teching, "Combat.State.Teching", "A tech animation is running. Projected by the defence tick extension while the tech state machine is in the tech state");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Knockdown, "Combat.State.Knockdown", "Downed after landing from a tumble; get-up abilities require it. Projected until KnockdownEndFrame by the defence tick extension");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Tech_Lockout, "Combat.State.Tech.Lockout", "A tech press is ignored, the anti-mash lockout. Projected while the frame is before TechLockoutEndFrame, for UI and AI");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mover_Tumble, "Combat.Mover.Tumble", "Mover tag carried by the tumble contact modifier while the entity tumbles. Added and removed with the modifier, never granted by an ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Immunity_Parry, "Combat.Immunity.Parry", "The attack cannot be parried, such as a boss signature attack. Granted by kits, abilities and gameplay effects as an ordinary count");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Immunity_Shield, "Combat.Immunity.Shield", "The attack cannot be blocked by the shield bubble. Granted by kits, abilities and gameplay effects as an ordinary count");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Defence_Shield, "Command.Defence.Shield", "Shield command, Hold(Shield). Part of the standard command set, built into a loadout only where the mode enables the shield");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Lock_Command_Defence, "Input.Lock.Command.Defence", "Locks the defence command family. An ordinary count granted by abilities and gameplay effects");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Blocked, "Combat.Event.Blocked", "A hit was blocked by the shield bubble. Authoritative event published once per key by the combat outbox on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Parried, "Combat.Event.Parried", "A hit was parried; the payload carries the source, shield or move. Authoritative event published once per key by the combat outbox");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_ShieldBroken, "Combat.Event.ShieldBroken", "The shield broke from damage. Authoritative event published once per key by the combat outbox on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Teched, "Combat.Event.Teched", "A tech succeeded. Authoritative event published once per key by the combat outbox on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_KnockedDown, "Combat.Event.KnockedDown", "An entity landed and is downed. Authoritative event published once per key by the combat outbox on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_ShieldDamage, "Combat.SetByCaller.ShieldDamage", "Gameplay effect magnitude key for the shield damage a blocked hit deals. Set by the damage code, read by the shield mirror effect");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			State_Shielding, State_ShieldStun, State_ShieldBroken, State_ParryWindow, State_ParryRecovery, State_Parried,
			State_Teching, State_Knockdown, State_Tech_Lockout,
			Mover_Tumble,
			Immunity_Parry, Immunity_Shield,
			Command_Defence_Shield, Input_Lock_Command_Defence,
			Event_Blocked, Event_Parried, Event_ShieldBroken, Event_Teched, Event_KnockedDown,
			SetByCaller_ShieldDamage });
	}
}
