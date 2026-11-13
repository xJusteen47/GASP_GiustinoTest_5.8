// Builds the trace record of one combat frame from the sync core (E17). The simulation tick calls it only while a trace sink is installed;
// it is a separate exported function so tests can check the mapping without a world.

#pragma once

#include "CoreMinimal.h"
#include "Trace/GSTN_CombatTrace.h"

struct FGSTN_CombatSyncCore;

namespace GSTNCombatTraceNP
{
	/** Const reads of the core as the frame left it. No query, no allocation: safe in a re-simulation and with no effect on the result */
	GSTNCOMBATNP_API FGSTN_CombatTraceFrame MakeFrame(int32 EntityId, int32 Frame, bool bResimulated, const FGSTN_CombatSyncCore& Core);
}
