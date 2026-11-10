// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldTags.h"

namespace GSTNHeldTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying, "Combat.Held.Carrying", "The entity holds an object. Projected every frame from its Hand link of type Held, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Pickup, "Command.Held.Pickup", "Pick up the nearest object in reach (grab button, nothing in hand). Default slot of UGSTN_HeldSlots");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Drop, "Command.Held.Drop", "Put the held object down (grab button again, requires Combat.Held.Carrying). Default slot of UGSTN_HeldSlots");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Throw, "Command.Held.Throw", "Throw the held object (strong button: tap = left stick, held = camera aim on release; requires Combat.Held.Carrying)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Target_Pickup, "Interact.Target.Pickup", "An object at rest is the interact focus. Projected by GSTNLink's interact probe from the object's GSTNInteract record (M10 G2); the pickup binding requires it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Type_Held, "Combat.Link.Type.Held", "A hold: the Hand-channel link of a holder and a held object (M10 G1)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Role_Holder, "Combat.Link.Role.Holder", "Role 1 of a Held link, projected on the holder");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Link_Role_HeldObject, "Combat.Link.Role.HeldObject", "Role 2 of a Held link: the object's side");
}
