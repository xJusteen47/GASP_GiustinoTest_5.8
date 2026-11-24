// Modular cosmetics, pure layer.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the cosmetics plugin. Slot and spline classes are hierarchical on purpose: a theme that has no piece for
 * Cosmetic.Slot.Grip.Pommel falls back to its Cosmetic.Slot.Grip piece, then to nothing (skin-cosmetici-modulari.md §3 rule 2).
 * These are the standard classes every item and theme agree on; a designer may add finer leaves under them in ini, never new roots.
 * Theme identities (Cosmetic.Theme.<Name>) are designer content: only the root is declared.
 */
namespace GSTNCosmeticTags
{
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Grip);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Grip_Pommel);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Guard);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_BladeBase);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Tip);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Charm);
	/** Body slots live on the one game skeleton as bone sockets, so a theme decorates any skin's armour the same way (phase 3) */
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Body);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Body_Shoulder);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Body_Belt);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Body_Wrist);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_Body_Back);

	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spline);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spline_Spine);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spline_Edge);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spline_Band);
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spline_Rim);

	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Theme);
	/** Root of style channel identities (colour, parts, emissive, reactive) */
	GSTNCOSMETICSCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Style);
}
