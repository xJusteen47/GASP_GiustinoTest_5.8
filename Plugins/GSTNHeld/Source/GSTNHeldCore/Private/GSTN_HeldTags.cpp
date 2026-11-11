// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldTags.h"

namespace GSTNHeldTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying, "Combat.Held.Carrying", "The entity holds an object. Projected every frame from its Hand link of type Held, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Size_Light, "Combat.Held.Size.Light", "Size class of an object: carried with every move (M10 G6)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Size_Medium, "Combat.Held.Size.Medium", "Size class of an object: no shield, no air attacks while carried (M10 G6 default bans)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Size_Heavy, "Combat.Held.Size.Heavy", "Size class of an object: walk, drop and throw only while carried (M10 G6 default bans)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Size_Huge, "Combat.Held.Size.Huge", "Size class of an object: as Heavy, and only a holder whose carry capacity covers its weight lifts it (M10 G6)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying_Light, "Combat.Held.Carrying.Light", "Holds a Light object. Projected from the Held link's payload, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying_Medium, "Combat.Held.Carrying.Medium", "Holds a Medium object. Projected from the Held link's payload, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying_Heavy, "Combat.Held.Carrying.Heavy", "Holds a Heavy object. Projected from the Held link's payload, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying_Huge, "Combat.Held.Carrying.Huge", "Holds a Huge object. Projected from the Held link's payload, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Held_Load, "Combat.SetByCaller.Held.Load", "Set-by-caller key of the carry load effect: the Held link's payload / 255 (size class and load ratio)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Pickup, "Command.Held.Pickup", "Pick up the nearest object in reach (grab button, nothing in hand). Default slot of UGSTN_HeldSlots");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Drop, "Command.Held.Drop", "Put the held object down (grab button again, requires Combat.Held.Carrying). Default slot of UGSTN_HeldSlots");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Throw, "Command.Held.Throw", "Throw the held object (strong button: tap = left stick, held = camera aim on release; requires Combat.Held.Carrying)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_ThrowEnemy, "Command.Held.ThrowEnemy", "Throw the lifted enemy (M10 G9: strong button as the held throw; requires Combat.Held.CarryingEnemy)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_CarryingEnemy, "Combat.Held.CarryingEnemy", "The entity grabs an enemy it can lift (M10 G9). Projected every frame from its Body link and the lift rules, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_NoLift, "Combat.Held.NoLift", "On an enemy: grabbable but never lifted and thrown as a held object (M10 G9)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_ThrownEnemy, "Combat.Source.Thrown.Enemy", "A thrown enemy made this hit (M10 G9); child of Combat.Source.Thrown");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Target_Pickup, "Interact.Target.Pickup", "An object at rest is the interact focus. Projected by GSTNLink's interact probe from the object's GSTNInteract record (M10 G2); the pickup binding requires it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Type_Held, "Combat.Link.Type.Held", "A hold: the Hand-channel link of a holder and a held object (M10 G1)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Role_Holder, "Combat.Link.Role.Holder", "Role 1 of a Held link, projected on the holder");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Role_HeldObject, "Combat.Link.Role.HeldObject", "Role 2 of a Held link: the object's side");
}
