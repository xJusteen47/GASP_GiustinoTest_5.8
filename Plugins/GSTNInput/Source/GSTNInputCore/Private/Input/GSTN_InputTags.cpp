// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputTags.h"

namespace GSTNInputTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button, "Input.Button", "Root of the logical button tags");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command, "Command", "Root of the command tags");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lock_Command, "Input.Lock.Command", "Locks every command; Input.Lock.Command.<root> locks Command.<root>.*");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Context_Grounded, "Input.Context.Grounded", "The character is on the ground");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Context_Airborne, "Input.Context.Airborne", "The character is in the air (falling or rising, not on a wall)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Context_OnWall, "Input.Context.OnWall", "The character is in a wall movement mode; with Airborne it identifies wall bindings");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BufferEvent, "Input.BufferEvent", "Root of the frame events reset rules listen to");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BufferEvent_AbilityActivated, "Input.BufferEvent.AbilityActivated", "An ability started from a command this frame");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BufferEvent_Landed, "Input.BufferEvent.Landed", "The character landed this frame");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BufferEvent_Interrupted, "Input.BufferEvent.Interrupted", "The character was interrupted from outside this frame (a hit received, a stun)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_CommandExecuted, "Input.Event.CommandExecuted", "A command was executed (published through the authoritative event stream)");
}
