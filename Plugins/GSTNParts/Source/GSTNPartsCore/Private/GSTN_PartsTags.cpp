// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_PartsTags.h"

namespace GSTNPartsTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Broken, "Combat.Part.Broken", "At least one part of the entity is broken. Projected every frame from the broken mask by the parts tick extension, never granted by an ability or effect");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_GrabLocked, "Combat.Part.GrabLocked", "The grab gate of the entity's part set is unmet, so the entity cannot be grabbed yet. Projected from the broken mask by the parts tick extension; the grab rules list it as an ignore requirement in data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_AllBroken, "Combat.Part.AllBroken", "Every breakable part of the entity is broken. Projected from the broken mask by the parts tick extension, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Part_Effect, "Combat.Part.Effect", "Root of the tags a Tags break effect grants while its part is broken, such as an arm-lost tag. Leaves are content; projected from the broken mask, never granted by others");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Destructible, "Combat.Entity.Destructible", "Marker in the hit victim view: the entity has a part set. Added by the parts fragment registration, read by hit steps and UI, never granted by an ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_Damaged, "Combat.Event.Part.Damaged", "A part took damage and did not ricochet. Published once per key by the combat outbox on the authority; read by UI and hit markers");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_WeakPointHit, "Combat.Event.Part.WeakPointHit", "A weak point part was hit and did not ricochet. Published once per key by the combat outbox on the authority; read by UI markers and audio");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_Ricochet, "Combat.Event.Part.Ricochet", "A hit landed on a penetration band that ricochets. Published once per key by the combat outbox; read by the spark cue and audio, the attacker recoil is its own ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_Broken, "Combat.Event.Part.Broken", "A part, or a child cascaded from a broken parent, broke. Published once per (entity, part index, break serial) by the combat outbox; read by presentation, UI and objectives");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Part_AllBroken, "Combat.Event.Part.AllBroken", "Every breakable part of the entity is broken. Published once per key by the combat outbox; an optional design hook");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_PartBroken, "Message.Combat.PartBroken", "Message router channel for the UI: a part broke. Republished from the outbox event outside the simulation by presentation or a UI bridge, never raised inside the simulation");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Part_Broken, Part_GrabLocked, Part_AllBroken, Part_Effect,
			Entity_Destructible,
			Event_Part_Damaged, Event_Part_WeakPointHit, Event_Part_Ricochet, Event_Part_Broken, Event_Part_AllBroken,
			Message_PartBroken });
	}
}
