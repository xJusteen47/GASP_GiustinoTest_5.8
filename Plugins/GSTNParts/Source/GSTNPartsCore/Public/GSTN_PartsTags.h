// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the parts plugin owns (combat-parts.md §7, modularity-and-tags 4.2).
 *
 * Combat.Part.Broken, GrabLocked and AllBroken are projected every frame from the broken-part mask by the plugin's tick
 * extension, never granted by an ability or an effect, so they stay deterministic across a rollback. Combat.Part.Effect.* leaves
 * (ArmLost...) are granted by the Tags break effect of the content, so only the root is native. Designer vocabulary is ini
 * only (Config/Tags/GSTNParts.ini): the Part.Type.* classification of a part and the GameplayCue.Combat.Part.* cues, and code
 * never names a leaf.
 *
 * The plugin declares no Combat.State.*, Combat.Mover.*, Combat.Lock.* or Combat.Link.* tag: stagger is the existing
 * Combat.State.Hitstun and hitstop the existing Combat.State.Hitstop. No part tag is ever stored in a sync state or a snapshot:
 * those carry the part mask and the part indices only.
 */
namespace GSTN_PartsTags
{
	/** At least one part of the entity is broken. Projected from the broken mask, never granted. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_Broken);

	/** The entity's grab gate is unmet (a negative tag: the grab rules list it as an ignore requirement). Projected, never granted. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_GrabLocked);

	/** Every breakable part of the entity is broken. Projected, never granted. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_AllBroken);

	/** Root of the tags a Tags break effect grants while a part is broken (Combat.Part.Effect.ArmLost...). Leaves are content. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_Effect);

	/** Marker in the hit victim view: the entity has a part set. Added by the plugin's fragment registration. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Destructible);

	/** A part took damage (not a ricochet). Published once per key by the combat outbox. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_Damaged);

	/** A part flagged as a weak point was hit and did not ricochet. Published once per key by the combat outbox. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_WeakPointHit);

	/** A hit landed on a band that ricochets. The attacker's recoil is its own ability, not this event. Published by the combat outbox. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_Ricochet);

	/** A part, or a part cascaded from a broken parent, broke. Deduplicated by (entity, part index, break serial). */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_Broken);

	/** Every breakable part is now broken. Optional design hook. Published once per key by the combat outbox. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_AllBroken);

	/** Message router channel for the UI: a part broke. Republished from the outbox event outside the simulation, never inside it. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_PartBroken);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNPARTSCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
