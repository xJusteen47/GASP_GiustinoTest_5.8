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
	/** The entity holds an object. Projected every frame from FGSTN_HeldFragment by the held tick extension, never granted */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Held_Carrying);

	/** Pick up the nearest object in reach: the grab button, with nothing in hand */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Held_Pickup);
	/** Put the held object down: the grab button again, with an object in hand */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Held_Drop);
	/** Throw the held object: the strong button (tap = stick, held = camera aim on release) */
	GSTNHELDCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Held_Throw);
}
