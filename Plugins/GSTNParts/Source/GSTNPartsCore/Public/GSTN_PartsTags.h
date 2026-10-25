// Pure part logic (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the parts plugin owns (combat-parts 7). The state tags are projected by the parts tick extension from the broken
 * mask, never granted by an ability or an effect, so they stay deterministic across a rollback. The event tags name outbox
 * events; the message tag is a UI channel. The designer vocabulary (Part.Type.*, GameplayCue.Combat.Part.*) is ini content in
 * Config/Tags/GSTNParts.ini and code never names a leaf.
 *
 * Parts never declare Combat.State.*, Combat.Mover.*, Combat.Lock.* or Combat.Link.*: stagger is the combat plugin's
 * Combat.State.Hitstun and hitstop its Combat.State.Hitstop. No tag goes into sync state or into a snapshot POD (part indices
 * and masks only).
 */
namespace GSTN_PartsTags
{
	// States, projected by the parts tick extension from the broken mask

	/** At least one part of the entity is broken. Projected, never granted. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_Broken);

	/** The grab gate of the part set is unmet, so the entity cannot be grabbed (read as data by the grab rules). Projected, never granted. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_GrabLocked);

	/** Every breakable part of the entity is broken. Projected, never granted. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_AllBroken);

	// Roots

	/** Root of the tags a break effect grants (for example Combat.Part.Effect.ArmLost). Leaves are designer content, code never names one. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Part_Effect);

	/** The entity has a part set: a marker in the hit victim view's tags, set when the parts fragment is registered on it. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Destructible);

	// Events, written to the combat outbox by the hit pipeline

	/** A hit damaged a part without breaking it. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_Damaged);

	/** A hit landed on a part flagged as a weak point. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_WeakPointHit);

	/** A hit bounced off a part (a ricochet band of the penetration table). */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_Ricochet);

	/** A part broke. Carries the part index and the break serial that deduplicates it across a rollback. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_Broken);

	/** The last breakable part broke. */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Part_AllBroken);

	// Messages, for the UI

	/** Gameplay message channel for a part that broke, posted by the presentation or a UI bridge (never by the simulation). */
	GSTNPARTSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_PartBroken);

	/** Every tag declared above, in declaration order. For the tests and the tag listings. */
	GSTNPARTSCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
