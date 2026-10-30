// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

/**
 * Registers and unregisters FGSTN_DefenceFragment under GSTNFeatureIds::Defence (id 17 of the central registry, GSTNCombatCore). The
 * module does it at startup and shutdown, before the first simulation tick, which is what makes the id mean something on this
 * machine (a machine without the plugin skips the fragment when it arrives). Exposed so a test can restore the registration it
 * changed.
 */
struct GSTNDEFENCENP_API FGSTN_DefenceFragmentRegistration
{
	/** True when the fragment is registered after the call (registering twice is fine) */
	static bool Register();

	static void Unregister();
};
