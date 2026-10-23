// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_LoadoutSwapLogic.h"

#include "Simulation/GSTN_CombatTimelineLogic.h"

EGSTN_LoadoutSwapResult FGSTN_LoadoutSwapLogic::Step(FGSTN_CombatSyncCore& Core, bool bDemandedTablePresent, int32 Frame)
{
	if (!bDemandedTablePresent)
	{
		return EGSTN_LoadoutSwapResult::Closed;
	}
	if (Core.TableRevision == Core.LoadoutRevision)
	{
		return EGSTN_LoadoutSwapResult::Unchanged;
	}

	// The old kit's move ends: the request names the old registry slot, which the pre-tick of this frame resolves against the old set
	FGSTN_CombatTimelineLogic::CancelForOtherCommand(Core, Frame);
	Core.ActiveWeaponIndex = 0;

	// Presses and stick anchors of earlier frames were made for the old kit; the current frame's own input is still live
	Core.Input.History.FlushPresses(~0u, Frame - 1);
	Core.Input.History.ConsumeSticksUpTo(Frame - 1);

	Core.TableRevision = Core.LoadoutRevision;
	return EGSTN_LoadoutSwapResult::Swapped;
}
