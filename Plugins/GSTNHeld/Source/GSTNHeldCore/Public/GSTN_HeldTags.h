// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * The native tags of GSTNHeld (M10). Commands are declared here like GSTNLink's Command.Link.*: the plugin owns its command family and its
 * default slots (UGSTN_HeldSlots), a kit rebinds them as data.
 */
namespace GSTNHeldTags
{
	/** The entity holds an object. Projected every frame from its Hand link of type Held (M10 G1) by the held tick extension, never granted */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Carrying);

	/**
	 * Size classes of an object (M10 G6, FGSTN_HeldSizeClass) and the Carrying.<size> tag projected beside Combat.Held.Carrying while the holder
	 * holds one of that class (decoded from the Held link's payload, so predicted and rolled back with the link). A size class's bans key on them
	 */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Size_Light);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Size_Medium);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Size_Heavy);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Size_Huge);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Carrying_Light);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Carrying_Medium);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Carrying_Heavy);
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Carrying_Huge);
	/** Set-by-caller key of the carry load effect: the Held link's payload / 255 (size class and load ratio, FGSTN_HeldLoad) */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Held_Load);

	/** Pick up the nearest object in reach: the grab button, with nothing in hand */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Held_Pickup);
	/** Put the held object down: the grab button again, with an object in hand */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Held_Drop);
	/** Throw the held object: the strong button (tap = stick, held = camera aim on release) */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Held_Throw);

	/**
	 * An object at rest is in pickup reach and the hand is empty (M10 slice 3, the plan's G2 meaning): projected every frame by the held tick
	 * extension, never granted. The pickup binding requires it, so with nothing in reach the grab button stays free for the slots below
	 */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Target_Pickup);

	/** The Hand-channel link type of a hold (M10 G1): the holder (role 1) and the object (role 2, a non-fighter partner) */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Link_Type_Held);
	/** Role 1 of a Held link: projected on the holder by GSTNLink's link tick */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Link_Role_Holder);
	/** Role 2 of a Held link: the object's side (never a fighter's tag; it tells the roles apart in the definition) */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Link_Role_HeldObject);
}
