// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldTags.h"

namespace GSTNHeldTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Held_Carrying, "Combat.Held.Carrying", "The entity holds an object. Projected every frame from the held fragment, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Pickup, "Command.Held.Pickup", "Pick up the nearest object in reach (grab button, nothing in hand). Default slot of UGSTN_HeldSlots");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Drop, "Command.Held.Drop", "Put the held object down (grab button again, requires Combat.Held.Carrying). Default slot of UGSTN_HeldSlots");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Held_Throw, "Command.Held.Throw", "Throw the held object (strong button: tap = left stick, held = camera aim on release; requires Combat.Held.Carrying)");
}
