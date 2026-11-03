// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

/**
 * Registers and unregisters FGSTN_PartsFragment under GSTNFeatureIds::Parts (id 16 of the central registry, GSTNCombatCore). The
 * module does it at startup and shutdown, before the first simulation tick, which is what makes the id mean something on this
 * machine (a machine without the plugin skips the fragment when it arrives). Exposed so a test can restore the registration it
 * changed.
 */
struct GSTNPARTSNP_API FGSTN_PartsFragmentRegistration
{
	/** True when the fragment is registered after the call (registering twice is fine) */
	static bool Register();

	static void Unregister();
};
