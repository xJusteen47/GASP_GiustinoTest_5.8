// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_SpinDrillTags.h"

namespace GSTNSpinDrillTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Movement_Spin_Clockwise, "Command.Movement.Spin.Clockwise", "Spin command, a clockwise stick rotation in Stick space. Declared by the spin and drill plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Movement_Spin_CounterClockwise, "Command.Movement.Spin.CounterClockwise", "Spin command, a counter-clockwise stick rotation in Stick space. Declared by the spin and drill plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Movement_Spin_Clockwise_Keys, "Command.Movement.Spin.Clockwise.Keys", "Clockwise spin from the keyboard shortcut D-S-A, a Motion in Stick space. Bound to the same abilities as the stick rotation");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Movement_Spin_CounterClockwise_Keys, "Command.Movement.Spin.CounterClockwise.Keys", "Counter-clockwise spin from the keyboard shortcut A-S-D, a Motion in Stick space. Bound to the same abilities as the stick rotation");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Movement_Spin_Button, "Command.Movement.Spin.Button", "Spin command from the optional spin button; the sense is the last stick rotation's, clockwise by default");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Movement_Drill, "Command.Movement.Drill", "Drill command, Hold(Crouch) read from the input history while a spin is active");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Button_Spin, "Input.Button.Spin", "Optional spin button, for players who prefer a button to a stick rotation. Not in the default kit");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Spinning, "Combat.State.Spinning", "A spin is active, either sense. Projected by the drill tick extension from the drill fragment, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Spinning_Clockwise, "Combat.State.Spinning.Clockwise", "A clockwise spin is active. Projected from the drill fragment, never granted; the opposite cancel keys on it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Spinning_CounterClockwise, "Combat.State.Spinning.CounterClockwise", "A counter-clockwise spin is active. Projected from the drill fragment, never granted; the opposite cancel keys on it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_AirSpinSpent, "Combat.State.AirSpinSpent", "The air spin was used during the current airtime, cleared on landing. Projected from the drill fragment, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Drilling, "Combat.State.Drilling", "The drill is diving or boring. Projected from the drill fragment, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_DrillLatched, "Combat.State.DrillLatched", "The drill is latched onto a target and boring. Projected from the drill fragment, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SpinLowGravity, "Combat.State.SpinLowGravity", "The low gravity of the air spin is on. Granted only by the low gravity effect, which the drill tick extension applies and removes from the fragment");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_SpinDrill_GravityScale, "Combat.SetByCaller.SpinDrill.GravityScale", "Multiplier of the jump and fall gravity scales while the air spin's low gravity is on");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_SpinDrill_FallSpeedScale, "Combat.SetByCaller.SpinDrill.FallSpeedScale", "Multiplier of the maximum fall speed while the air spin's low gravity is on");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Drill, "Combat.HitType.Drill", "A hit from the drill tip; carries a penetration compared with the hardness of the part it hits. Set on the hit data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Drill_Latched, "Combat.Event.Drill.Latched", "The drill latched onto a target. Published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Drill_Detached, "Combat.Event.Drill.Detached", "The drill detached from its target: release, maximum time, stagger or target gone. Published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Drill_Launched, "Combat.Event.Drill.Launched", "The drill ended in the high jump out of the target. Published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Drill_GroundLag, "Combat.Event.Drill.GroundLag", "The dive reached the ground and the ground lag started. Published once on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Spin, "Ability.Movement.Spin", "Identity of the spin ability, on the ground and in the air");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_SpinCancel, "Ability.Movement.SpinCancel", "Identity of the opposite-spin cancel ability, which stops an active spin early");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_Drill_Dive, "Ability.Combat.Drill.Dive", "Identity of the drill dive ability, the plunge along gravity");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_Drill_Bore, "Ability.Combat.Drill.Bore", "Identity of the drill bore ability, the multi-hit while latched");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_Drill_Launch, "Ability.Combat.Drill.Launch", "Identity of the drill launch ability, the high jump out of the target");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Command_Movement_Spin_Clockwise, Command_Movement_Spin_CounterClockwise, Command_Movement_Spin_Clockwise_Keys, Command_Movement_Spin_CounterClockwise_Keys,
			Command_Movement_Spin_Button, Command_Movement_Drill,
			Input_Button_Spin,
			State_Spinning, State_Spinning_Clockwise, State_Spinning_CounterClockwise, State_AirSpinSpent, State_Drilling, State_DrillLatched,
			State_SpinLowGravity, SetByCaller_SpinDrill_GravityScale, SetByCaller_SpinDrill_FallSpeedScale,
			HitType_Drill,
			Event_Drill_Latched, Event_Drill_Detached, Event_Drill_Launched, Event_Drill_GroundLag,
			Ability_Movement_Spin, Ability_Movement_SpinCancel, Ability_Combat_Drill_Dive, Ability_Combat_Drill_Bore, Ability_Combat_Drill_Launch });
	}
}
