// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"

/**
 * The fixed sync fragment type ids of the parts plugin. They go on the wire, so every machine of a match must agree on them: an id
 * is assigned once, never renumbered and never reused (combat extension surface, modularity-and-tags 5b). 1 to 127 are the GSTN
 * range of GSTNFeatureIds (Sync/GSTN_SyncFragment.h).
 *
 * The central registry file is in GSTNCombatCore, which this plugin must not edit, so the id is declared here for now and the line
 * below has to move into GSTNFeatureIds when that file is next touched. The value is reserved by this plugin: no other plugin may
 * take 16 (grep gate: nothing else in the repository names it; GSTNFeatureIds holds only the test ids 240 to 242).
 */
namespace GSTNPartsFeatureIds
{
	/** FGSTN_PartsFragment */
	constexpr uint8 Parts = 16;
}

/**
 * Registers and unregisters FGSTN_PartsFragment under GSTNPartsFeatureIds::Parts. The module does it at startup and shutdown, before
 * the first simulation tick, which is what makes the id mean something on this machine (a machine without the plugin skips the
 * fragment when it arrives). Exposed so a test can restore the registration it changed.
 */
struct GSTNPARTSNP_API FGSTN_PartsFragmentRegistration
{
	/** True when the fragment is registered after the call (registering twice is fine) */
	static bool Register();

	static void Unregister();
};
