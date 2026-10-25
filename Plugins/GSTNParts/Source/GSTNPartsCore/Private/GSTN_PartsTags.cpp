// Pure part logic (no GAS, no Network Prediction, no Mover).

#include "GSTN_PartsTags.h"

namespace GSTN_PartsTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Broken, "Combat.Part.Broken", "At least one part is broken. Projected by the parts tick extension from the broken mask, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_GrabLocked, "Combat.Part.GrabLocked", "The grab gate of the part set is unmet. Projected by the parts tick extension, never granted; the grab rules list it as data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_AllBroken, "Combat.Part.AllBroken", "Every breakable part is broken. Projected by the parts tick extension, never granted");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Effect, "Combat.Part.Effect", "Root of the tags a break effect grants. Leaves are designer content in a tag ini, code never names one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Destructible, "Combat.Entity.Destructible", "The entity has a part set. Marker in the hit victim view's tags, set when the parts fragment is registered");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_Damaged, "Combat.Event.Part.Damaged", "A hit damaged a part without breaking it. Written to the combat outbox by the hit pipeline");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_WeakPointHit, "Combat.Event.Part.WeakPointHit", "A hit landed on a weak point part. Written to the combat outbox by the hit pipeline");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_Ricochet, "Combat.Event.Part.Ricochet", "A hit bounced off a part. Written to the combat outbox by the hit pipeline");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_Broken, "Combat.Event.Part.Broken", "A part broke. Written to the combat outbox with the break serial that deduplicates it across a rollback");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_AllBroken, "Combat.Event.Part.AllBroken", "The last breakable part broke. Written to the combat outbox by the hit pipeline");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_PartBroken, "Message.Combat.PartBroken", "Gameplay message channel for a part that broke. Posted by the presentation or a UI bridge, never by the simulation");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Part_Broken, Part_GrabLocked, Part_AllBroken, Part_Effect, Entity_Destructible,
			Event_Part_Damaged, Event_Part_WeakPointHit, Event_Part_Ricochet, Event_Part_Broken, Event_Part_AllBroken,
			Message_PartBroken });
	}
}
