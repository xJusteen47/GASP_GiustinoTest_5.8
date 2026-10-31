// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformTags.h"

namespace GSTNPlatformTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Platform, "Platform", "Root of the platform tags owned by GSTNPlatform. Presentation and designer queries only, never in a sync state");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_State, "Platform.State", "Root of the platform shape states. Derived from the shape timeline on demand, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Active, "Platform.State.Active", "The platform shape is solid and posed with its enemy");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Braced, "Platform.State.Braced", "Reserved: the platform shape is solid and plays its brace telegraph (mount)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Breaking, "Platform.State.Breaking", "The platform shape is still solid and switches off at the end of its drop grace");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Inactive, "Platform.State.Inactive", "The platform shape has no collision");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Event, "Platform.Event", "Root of the local platform presentation events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Mounted, "Platform.Event.Mounted", "Reserved local presentation trigger: a character started using a platform shape");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Dropped, "Platform.Event.Dropped", "Reserved local presentation trigger: a platform shape a character used went away");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Root_Platform, Root_State, State_Active, State_Braced, State_Breaking, State_Inactive,
			Root_Event, Event_Mounted, Event_Dropped });
	}
}
